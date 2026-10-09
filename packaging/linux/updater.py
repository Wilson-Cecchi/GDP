#!/usr/bin/env python3
"""Opt-in Linux updater; never replaces a running game or its save directory."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request


REPOSITORY = "Wilson-Cecchi/Iron-Seed-Downloads"
MANIFEST_URL = f"https://github.com/{REPOSITORY}/releases/latest/download/update.json"
SIGNATURE_URL = MANIFEST_URL + ".sig"
PUBLIC_KEY = Path(__file__).with_name("release-signing-public.pem")
ARCHIVE_NAME = "IronSeed-Linux-x64.tar.gz"
PACKAGE_NAME = "IronSeed-Linux-x64"
BRAINS = ("worker", "guard", "officer", "engineer", "robot")
MAX_ARCHIVE_BYTES = 512 * 1024 * 1024
MAX_EXPANDED_BYTES = 1024 * 1024 * 1024
MAX_MANIFEST_BYTES = 65536
MAX_SIGNATURE_BYTES = 8192


class UpdateError(Exception):
    pass


def version_parts(value: str) -> tuple[int, int, int]:
    if not isinstance(value, str) or not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", value):
        raise UpdateError("invalid version")
    parts = tuple(int(part) for part in value.split("."))
    if any(part > 999999 for part in parts):
        raise UpdateError("version is too large")
    return parts


def parse_manifest(raw: bytes, current_version: str) -> dict | None:
    try:
        document = json.loads(raw)
        if not isinstance(document, dict):
            raise TypeError("manifest must be an object")
        version = document["version"]
        tag = document["tag"]
        asset = document["assets"]["linux-x64"]
    except (ValueError, KeyError, TypeError, AttributeError) as exc:
        raise UpdateError("invalid release manifest") from exc
    source_sha = document.get("source_sha")
    if (document.get("schema_version") != 1 or not isinstance(tag, str)
        or not isinstance(source_sha, str) or tag != f"public-v{version}"
        or not re.fullmatch(r"[0-9a-f]{40}", source_sha)):
        raise UpdateError("unrecognized release manifest")
    if version_parts(version) <= version_parts(current_version):
        return None
    expected_url = f"https://github.com/{REPOSITORY}/releases/download/{tag}/{ARCHIVE_NAME}"
    if not isinstance(asset, dict) or asset.get("file") != ARCHIVE_NAME \
        or asset.get("url") != expected_url \
        or not isinstance(asset.get("sha256"), str) \
        or not re.fullmatch(r"[0-9a-f]{64}", asset["sha256"]) \
        or type(asset.get("size")) is not int \
        or not 0 < asset["size"] <= MAX_ARCHIVE_BYTES:
        raise UpdateError("invalid Linux release asset")
    return {"version": version, "url": expected_url,
            "sha256": asset["sha256"], "size": asset["size"]}


def fetch_limited(url: str, limit: int) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "IronSeed-Linux-Updater"})
    with urllib.request.urlopen(request, timeout=8) as response:
        raw = response.read(limit + 1)
    if not raw or len(raw) > limit:
        raise UpdateError("release metadata is empty or too large")
    return raw


def verify_manifest_signature(raw: bytes, signature: bytes) -> None:
    if not PUBLIC_KEY.is_file():
        raise UpdateError("release verification key is missing")
    with tempfile.TemporaryDirectory(prefix="iron-seed-signature-") as temporary:
        manifest_path = Path(temporary) / "update.json"
        signature_path = Path(temporary) / "update.json.sig"
        manifest_path.write_bytes(raw)
        signature_path.write_bytes(signature)
        result = subprocess.run(
            ["openssl", "dgst", "-sha256", "-verify", str(PUBLIC_KEY),
             "-signature", str(signature_path), "-sigopt", "rsa_padding_mode:pss",
             "-sigopt", "rsa_pss_saltlen:-1", str(manifest_path)],
            capture_output=True, timeout=10, check=False)
    if result.returncode != 0:
        raise UpdateError("release manifest signature is invalid")


def fetch_manifest(current_version: str) -> dict | None:
    raw = fetch_limited(MANIFEST_URL, MAX_MANIFEST_BYTES)
    signature = fetch_limited(SIGNATURE_URL, MAX_SIGNATURE_BYTES)
    verify_manifest_signature(raw, signature)
    return parse_manifest(raw, current_version)


def download_archive(asset: dict, destination: Path) -> None:
    request = urllib.request.Request(asset["url"], headers={"User-Agent": "IronSeed-Linux-Updater"})
    digest = hashlib.sha256()
    size = 0
    with urllib.request.urlopen(request, timeout=30) as response, destination.open("wb") as output:
        while chunk := response.read(1024 * 1024):
            size += len(chunk)
            if size > asset["size"] or size > MAX_ARCHIVE_BYTES:
                raise UpdateError("download exceeds declared size")
            digest.update(chunk)
            output.write(chunk)
    if size != asset["size"] or digest.hexdigest() != asset["sha256"]:
        raise UpdateError("download size or SHA-256 mismatch")


def extract_archive(archive_path: Path, destination: Path) -> Path:
    seen: set[str] = set()
    total = 0
    with tarfile.open(archive_path, "r:gz") as archive:
        members = archive.getmembers()
        if len(members) > 1000:
            raise UpdateError("too many archive entries")
        for member in members:
            name = member.name.rstrip("/")
            path = PurePosixPath(name)
            if not name or path.is_absolute() or ".." in path.parts \
                or path.parts[0] != PACKAGE_NAME or "\\" in name \
                or any(ord(char) < 32 for char in name) or name in seen \
                or not (member.isdir() or member.isfile()):
                raise UpdateError("unsafe archive entry")
            seen.add(name)
            total += member.size
            if total > MAX_EXPANDED_BYTES:
                raise UpdateError("archive expands beyond limit")
        for member in members:
            target = destination.joinpath(*PurePosixPath(member.name).parts)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                stream = archive.extractfile(member)
                if stream is None:
                    raise UpdateError("archive file cannot be read")
                with stream, target.open("xb") as output:
                    shutil.copyfileobj(stream, output)
                mode = 0o755 if target.name in ("gdp", "updater.py") else 0o644
                target.chmod(mode)
    return destination / PACKAGE_NAME


def verify_package(package: Path, version: str, require_verification_key: bool = True) -> None:
    executable = package / "bin" / "gdp"
    if not executable.is_file() or not (package / "gdp").is_file() \
        or not (package / "updater.py").is_file() \
        or (require_verification_key and not (package / "release-signing-public.pem").is_file()) \
        or (package / "version.txt").read_text(encoding="utf-8").strip() != version:
        raise UpdateError("incomplete game package")
    for brain in BRAINS:
        if not (package / "bin" / "ai" / f"{brain}.brain").is_file():
            raise UpdateError(f"missing {brain} brain")
    result = subprocess.run([str(executable), "--version"], capture_output=True,
                            text=True, timeout=10, check=False)
    if result.returncode != 0 or result.stdout.strip() != f"Iron Seed {version}":
        raise UpdateError("packaged game version mismatch")
    result = subprocess.run([str(executable), "--headless", "--frames", "3"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                            timeout=30, check=False)
    if result.returncode != 0:
        raise UpdateError("packaged game smoke test failed")


def install_update(root: Path, asset: dict) -> None:
    versions = root / "versions"
    if versions.is_symlink():
        raise UpdateError("versions directory cannot be a symbolic link")
    versions.mkdir(exist_ok=True)
    target_parent = versions / asset["version"]
    target = target_parent / PACKAGE_NAME
    if target_parent.is_symlink() or target.is_symlink():
        raise UpdateError("version path cannot be a symbolic link")
    if not target.exists():
        if target_parent.exists():
            raise UpdateError("version directory already exists")
        with tempfile.TemporaryDirectory(prefix=".stage-", dir=versions) as staging:
            archive_path = Path(staging) / ARCHIVE_NAME
            download_archive(asset, archive_path)
            package = extract_archive(archive_path, Path(staging) / "unpacked")
            verify_package(package, asset["version"])
            target_parent.mkdir()
            try:
                os.replace(package, target)
            except OSError:
                target_parent.rmdir()
                raise
    else:
        verify_package(target, asset["version"])
    marker = root / "current-version.txt"
    previous = marker.read_text(encoding="ascii").strip() if marker.exists() else "base"
    if previous != "base":
        version_parts(previous)
    write_marker(root, "previous-version.txt", previous)
    write_marker(root, "current-version.txt", asset["version"])


def write_marker(root: Path, name: str, value: str) -> None:
    marker = root / name
    with tempfile.NamedTemporaryFile(mode="w", encoding="ascii", dir=root,
                                     prefix=f".{name}-", delete=False) as temporary:
        temporary.write(value + "\n")
        temporary_path = Path(temporary.name)
    try:
        os.replace(temporary_path, marker)
    finally:
        temporary_path.unlink(missing_ok=True)


def rollback_update(root: Path) -> None:
    previous = (root / "previous-version.txt").read_text(encoding="ascii").strip()
    if previous == "base":
        package = root
        version_parts((package / "version.txt").read_text(encoding="ascii").strip())
    else:
        version_parts(previous)
        package = root / "versions" / previous / PACKAGE_NAME
        if not package.is_dir() or package.is_symlink():
            raise UpdateError("previous version is unavailable")
    verify_package(package, (package / "version.txt").read_text(encoding="ascii").strip(),
                   require_verification_key=False)
    write_marker(root, "current-version.txt", previous)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install-root", required=True, type=Path)
    parser.add_argument("--current-package", required=True, type=Path)
    parser.add_argument("--rollback", action="store_true")
    args = parser.parse_args()
    try:
        root = args.install_root.resolve(strict=True)
        current = args.current_package.resolve(strict=True)
        if current != root and root not in current.parents:
            raise UpdateError("package is outside its installation")
        if args.rollback:
            rollback_update(root)
            return 0
        current_version = (current / "version.txt").read_text(encoding="utf-8").strip()
        version_parts(current_version)
        asset = fetch_manifest(current_version)
        if asset is None:
            return 0
        prompt = subprocess.run([str(current / "bin" / "gdp"), "--confirm-update", asset["version"]],
                                timeout=300, check=False)
        if prompt.returncode != 0:
            return 0
        install_update(root, asset)
        return 0
    except (OSError, ValueError, UpdateError, subprocess.TimeoutExpired) as exc:
        print(f"Iron Seed updater: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

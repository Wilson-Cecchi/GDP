#!/usr/bin/env python3
"""Describe one immutable Iron Seed release for future update checks."""

import argparse
import hashlib
import json
from pathlib import Path
import re


DOWNLOADS_REPO = "Wilson-Cecchi/Iron-Seed-Downloads"
ASSETS = {
    "windows-x64": "IronSeed-Windows-x64.zip",
    "linux-x64": "IronSeed-Linux-x64.tar.gz",
    "macos-intel": "IronSeed-macOS-Intel.zip",
    "macos-apple-silicon": "IronSeed-macOS-AppleSilicon.zip",
}


def make_manifest(tag: str, source_sha: str, directory: Path) -> dict:
    if not re.fullmatch(r"public-v\d+\.\d+\.\d+", tag):
        raise ValueError("release tag must be public-vMAJOR.MINOR.PATCH")
    if not re.fullmatch(r"[0-9a-f]{40}", source_sha):
        raise ValueError("source SHA must be 40 lowercase hexadecimal characters")

    actual = {path.name for path in directory.iterdir() if path.is_file()}
    expected = set(ASSETS.values())
    if actual != expected:
        raise ValueError(f"expected exactly {sorted(expected)}; found {sorted(actual)}")

    assets = {}
    for platform, name in ASSETS.items():
        path = directory / name
        digest = hashlib.sha256()
        size = 0
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
                size += len(chunk)
        if size == 0:
            raise ValueError(f"empty release asset: {name}")
        assets[platform] = {
            "file": name,
            "size": size,
            "sha256": digest.hexdigest(),
            "url": f"https://github.com/{DOWNLOADS_REPO}/releases/download/{tag}/{name}",
        }

    return {
        "schema_version": 1,
        "version": tag.removeprefix("public-v"),
        "tag": tag,
        "source_sha": source_sha,
        "assets": assets,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--directory", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    manifest = make_manifest(args.tag, args.source_sha, args.directory)
    args.output.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()

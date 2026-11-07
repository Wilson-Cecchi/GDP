#!/usr/bin/env python3
"""Check that a portable package contains the current runtime visual assets."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


RUNTIME_SUFFIXES = {".png", ".isemesh", ".isplayer"}


def digest(path: Path) -> bytes:
    with path.open("rb") as stream:
        checksum = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
        return checksum.digest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("package", type=Path)
    args = parser.parse_args()
    if not args.source.is_dir() or not args.package.is_dir():
        parser.error("source and package visual directories must exist")

    source_files = sorted(
        path for path in args.source.rglob("*")
        if path.is_file() and path.suffix.lower() in RUNTIME_SUFFIXES
    )
    if not source_files:
        parser.error("source contains no runtime visual assets")

    failures = []
    for source in source_files:
        relative = source.relative_to(args.source)
        installed = args.package / relative
        if not installed.is_file():
            failures.append(f"missing: {relative}")
        elif source.stat().st_size != installed.stat().st_size or digest(source) != digest(installed):
            failures.append(f"different: {relative}")

    if failures:
        print("\n".join(failures))
        return 1
    print(f"Verified {len(source_files)} runtime visual assets")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

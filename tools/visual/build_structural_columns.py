#!/usr/bin/env python3
"""Build deterministic seamless albedo tiles for structural room columns."""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path


SIZE = 256
PERIOD = SIZE - 1
DEFAULT_OUTPUT = Path(__file__).resolve().parents[2] / "assets/visual/rooms/columns"
CONDITIONS = ("maintained", "used", "worn")


def noise(x: int, y: int, seed: int) -> int:
    value = (x * 0x9E3779B1 + y * 0x85EBCA77 + seed) & 0xFFFFFFFF
    value ^= value >> 16
    value = (value * 0x7FEB352D) & 0xFFFFFFFF
    value ^= value >> 15
    return value & 0xFF


def render(condition: int) -> bytes:
    pixels = bytearray(SIZE * SIZE * 3)
    for y in range(PERIOD):
        for x in range(PERIOD):
            grain = noise(x, y, 0xC011) % 9 - 4
            broad = noise(x // 8, y // 8, 0x5A17) % 7 - 3
            value = 231 - condition * 6 + grain + broad
            # Two shallow horizontal joints per 0.8 m tile, plus restrained
            # vertical brushed ribs. Both are measured in metres by the shader.
            joint = min(y % 127, 127 - y % 127)
            if joint <= 2:
                value -= 33 - condition * 2
            rib = x % 64
            if rib <= 2:
                value -= 9
            elif rib in (3, 4):
                value += 7
            # Small, sparse fasteners stay inside the tile, away from edges.
            for cx in (32, 160):
                for cy in (16, 143):
                    dx, dy = x - cx, y - cy
                    distance = dx * dx + dy * dy
                    if distance <= 9:
                        value = 112 if distance <= 2 else 177
            if condition:
                wear = noise(x // 3, y // 5, 0xDEF0) % 127
                if wear < condition * 2 and 8 < x < 247 and 8 < y < 247:
                    value -= 22 + condition * 6
            value = max(0, min(255, value))
            offset = (y * SIZE + x) * 3
            pixels[offset:offset + 3] = bytes((value, value, value))
        row = y * SIZE * 3
        pixels[row + PERIOD * 3:row + SIZE * 3] = pixels[row:row + 3]
    pixels[PERIOD * SIZE * 3:] = pixels[:SIZE * 3]
    return bytes(pixels)


def chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload
            + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))


def png(rgb: bytes) -> bytes:
    stride = SIZE * 3
    rows = bytearray()
    for y in range(SIZE):
        rows.append(0)
        rows.extend(rgb[y * stride:(y + 1) * stride])
    header = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(bytes(rows), level=9))
            + chunk(b"IEND", b""))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for index, name in enumerate(CONDITIONS):
        target = args.output_dir / f"structural_{name}.png"
        target.write_bytes(png(render(index)))
        print(target)


if __name__ == "__main__":
    main()

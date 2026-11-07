#!/usr/bin/env python3
"""Build seamless, 0.75-metre RGB albedo tiles for Cafeteria furniture.

The approved concept boards contain perspective and lighting, so this generator
creates original material colour maps instead of cropping those boards. Mesh
edges, cabinet seams, fasteners and handles belong in geometry/local overlays.
"""

from __future__ import annotations

import argparse
import random
import struct
import zlib
from pathlib import Path


SIZE = 512
PERIOD = SIZE - 1  # opposing image borders sample the same point on the torus
DEFAULT_OUTPUT = Path(__file__).resolve().parents[2] / "assets/visual/rooms/cafeteria"
CONDITIONS = ("clean", "inuse", "faded")
MATERIALS = {
    # Neutral albedos: no lamp highlights, shadows, cabinet hardware or UV borders.
    "steel": ((170, 172, 171), 0x51EE1),
    "seat": ((118, 37, 35), 0x5EA71),
    "support": ((42, 45, 44), 0x5A770),
    "cabinet": ((65, 61, 57), 0xC0A71),
}


def hash01(x: int, y: int, seed: int) -> float:
    value = (x * 0x9E3779B1 + y * 0x85EBCA77 + seed) & 0xFFFFFFFF
    value ^= value >> 16
    value = (value * 0x7FEB352D) & 0xFFFFFFFF
    value ^= value >> 15
    value = (value * 0x846CA68B) & 0xFFFFFFFF
    value ^= value >> 16
    return value / 0xFFFFFFFF


def smooth(t: float) -> float:
    return t * t * (3.0 - 2.0 * t)


def periodic_field(seed: int, cells: int = 17) -> list[float]:
    """Small-scale colour variation with identical left/right and top/bottom."""
    lattice = [[hash01(x, y, seed) for x in range(cells)] for y in range(cells)]
    axis = []
    for position in range(SIZE):
        coordinate = position * cells / PERIOD
        index = int(coordinate)
        axis.append((index % cells, smooth(coordinate - index)))
    result = [0.0] * (SIZE * SIZE)
    for y, (y0, ty) in enumerate(axis):
        y1 = (y0 + 1) % cells
        top, bottom = lattice[y0], lattice[y1]
        row = y * SIZE
        for x, (x0, tx) in enumerate(axis):
            x1 = (x0 + 1) % cells
            a = top[x0] + (top[x1] - top[x0]) * tx
            b = bottom[x0] + (bottom[x1] - bottom[x0]) * tx
            result[row + x] = (a + (b - a) * ty) - 0.5
    return result


def brushed_grain(seed: int) -> list[int]:
    """Short, local brushed-steel marks; never a stripe across an entire tile."""
    grain = [0] * (SIZE * SIZE)
    rng = random.Random(seed ^ 0xB255)
    for _ in range(1050):
        x0 = rng.randrange(PERIOD)
        y0 = rng.randrange(PERIOD)
        length = rng.randrange(5, 49)
        shade = rng.randrange(3, 12) * rng.choice((-1, 1))
        slope = rng.uniform(-0.08, 0.08)
        for step in range(length):
            x = (x0 + step) % PERIOD
            y = (y0 + round(step * slope)) % PERIOD
            index = y * SIZE + x
            grain[index] = max(-15, min(15, grain[index] + shade))
    return grain


def wear_mask(material: str, condition: str, seed: int) -> bytearray:
    mask = bytearray(SIZE * SIZE)
    if condition == "clean":
        return mask
    rng = random.Random(seed + (15 if condition == "inuse" else 73))
    count = 19 if condition == "inuse" else 52
    if material == "steel":
        count += 12
    for _ in range(count):
        x0 = rng.randrange(PERIOD)
        y0 = rng.randrange(PERIOD)
        length = rng.randrange(12, 83 if condition == "inuse" else 135)
        slope = rng.uniform(-0.13, 0.13) if material == "steel" else rng.uniform(-1.2, 1.2)
        strength = rng.randrange(22, 62 if condition == "inuse" else 95)
        for step in range(length):
            x = (x0 + step) % PERIOD
            y = (y0 + round(step * slope)) % PERIOD
            index = y * SIZE + x
            mask[index] = max(mask[index], strength)
    if condition == "faded" and material != "steel":
        # Small paint nicks; never a painted border that would repeat on every UV edge.
        for _ in range(18):
            cx = rng.randrange(PERIOD)
            cy = rng.randrange(PERIOD)
            rx = rng.randrange(2, 6)
            ry = rng.randrange(1, 4)
            for dy in range(-ry, ry + 1):
                for dx in range(-rx, rx + 1):
                    if (dx / rx) ** 2 + (dy / ry) ** 2 <= 1:
                        x, y = (cx + dx) % PERIOD, (cy + dy) % PERIOD
                        index = y * SIZE + x
                        mask[index] = max(mask[index], 105)
    return mask


def clamp(value: float) -> int:
    return max(0, min(255, round(value)))


def render(material: str, condition: str, base: tuple[int, int, int], seed: int) -> bytes:
    field = periodic_field(seed)
    micro_field = periodic_field(seed ^ 0xB255, cells=79)
    wear = wear_mask(material, condition, seed)
    grain = brushed_grain(seed) if material == "steel" else None
    shift = {"clean": 0.0, "inuse": -2.0, "faded": -6.0}[condition]
    field_amount = {"clean": 4.0, "inuse": 7.0, "faded": 10.0}[condition]
    out = bytearray(SIZE * SIZE * 3)
    for y in range(PERIOD):
        for x in range(PERIOD):
            index = y * SIZE + x
            speckle = (hash01(x, y, seed ^ 0xD177) - 0.5) * (
                4.5 if material == "steel" else 3.0
            )
            variation = (
                field[index] * field_amount
                + micro_field[index] * (7.0 if material == "steel" else 3.5)
                + speckle
                + shift
            )
            if grain is not None:
                variation += grain[index]
            scratch = wear[index] / 255.0
            if material == "steel":
                # Fine scratches are darker in albedo; specular response is a renderer concern.
                variation -= scratch * 22.0
                colour = [clamp(channel + variation) for channel in base]
            else:
                exposed = (82, 82, 77) if material == "seat" else (87, 85, 80)
                coverage = scratch * (0.42 if condition == "inuse" else 0.77)
                colour = [
                    clamp((channel + variation) * (1.0 - coverage) + metal * coverage)
                    for channel, metal in zip(base, exposed)
                ]
            out[index * 3 : index * 3 + 3] = bytes(colour)
        # The final column is exactly the first column, including chips and scratches.
        start = y * SIZE * 3
        out[start + PERIOD * 3 : start + SIZE * 3] = out[start : start + 3]
    # The final row is exactly the first row.
    out[PERIOD * SIZE * 3 :] = out[: SIZE * 3]
    return bytes(out)


def chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(
        ">I", zlib.crc32(kind + payload) & 0xFFFFFFFF
    )


def png(rgb: bytes) -> bytes:
    rows = bytearray()
    stride = SIZE * 3
    for y in range(SIZE):
        rows.append(0)  # PNG filter None, stable across platforms
        rows.extend(rgb[y * stride : (y + 1) * stride])
    header = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", header)
        + chunk(b"IDAT", zlib.compress(bytes(rows), level=9))
        + chunk(b"IEND", b"")
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for material, (base, seed) in MATERIALS.items():
        for condition in CONDITIONS:
            target = args.output_dir / f"furniture_{material}_{condition}.png"
            target.write_bytes(png(render(material, condition, base, seed)))
            print(target)


if __name__ == "__main__":
    main()

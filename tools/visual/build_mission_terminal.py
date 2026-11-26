#!/usr/bin/env python3
"""Build the selected two-sided portal terminal as OBJ and ISEMESH1.

Run from the repository root: python3 tools/visual/build_mission_terminal.py
The Python geometry is the source of truth and needs no Blender installation.
The OBJ can be imported into Blender, but edits there need a separate export.
"""

from __future__ import annotations

from collections import defaultdict
from math import cos, pi, sin, sqrt
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets/visual/models/mission_terminal"
OUT.mkdir(parents=True, exist_ok=True)

# 0 paint, 1 graphite, 2 exposed metal, 3 gasket, 4 glass, 5 light.
# Part 1 is warm-white pillar lighting; part 2 is cyan screen/status lighting.
groups: dict[tuple[int, int], tuple[list[tuple[float, ...]], list[int]]] = defaultdict(
    lambda: ([], []))


def add_triangle(part: int, material: int, a, b, c) -> None:
    ab = tuple(b[i] - a[i] for i in range(3))
    ac = tuple(c[i] - a[i] for i in range(3))
    n = (ab[1] * ac[2] - ab[2] * ac[1],
         ab[2] * ac[0] - ab[0] * ac[2],
         ab[0] * ac[1] - ab[1] * ac[0])
    length = sqrt(sum(v * v for v in n))
    if length < 1e-8:
        raise ValueError("degenerate terminal triangle")
    n = tuple(v / length for v in n)
    if abs(n[1]) > 0.8:
        axes = (0, 2)
    elif abs(n[0]) > 0.8:
        axes = (2, 1)
    else:
        axes = (0, 1)
    strip = {0: (.02, .48), 1: (.02, .48), 2: (.52, .66),
             3: (.69, .74), 4: (.02, .48), 5: (.77, .98)}[material]
    base = (min(p[axes[0]] for p in (a, b, c)),
            min(p[axes[1]] for p in (a, b, c)))
    vertices, indices = groups[part, material]
    for p in (a, b, c):
        u = strip[0] + (p[axes[0]] - base[0]) / .75 * (strip[1] - strip[0])
        v = .02 + (p[axes[1]] - base[1]) / .75 * .96
        indices.append(len(vertices))
        vertices.append((*p, *n, u, v))


def add_quad(part: int, material: int, a, b, c, d) -> None:
    add_triangle(part, material, a, b, c)
    add_triangle(part, material, a, c, d)


def box(material: int, center, size, part: int = 0) -> None:
    x, y, z = center
    sx, sy, sz = (value * .5 for value in size)
    x0, x1, y0, y1, z0, z1 = x-sx, x+sx, y-sy, y+sy, z-sz, z+sz
    add_quad(part, material, (x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1))
    add_quad(part, material, (x1,y0,z0),(x0,y0,z0),(x0,y1,z0),(x1,y1,z0))
    add_quad(part, material, (x0,y0,z0),(x0,y0,z1),(x0,y1,z1),(x0,y1,z0))
    add_quad(part, material, (x1,y0,z1),(x1,y0,z0),(x1,y1,z0),(x1,y1,z1))
    add_quad(part, material, (x0,y1,z1),(x1,y1,z1),(x1,y1,z0),(x0,y1,z0))
    add_quad(part, material, (x0,y0,z0),(x1,y0,z0),(x1,y0,z1),(x0,y0,z1))


def chamfered_panel(material: int, center, size, cut: float, part: int = 0) -> None:
    x, y, z = center
    w, h, d = size
    hx, hy = w * .5, h * .5
    outline = [(-hx+cut,-hy),(hx-cut,-hy),(hx,-hy+cut),(hx,hy-cut),
               (hx-cut,hy),(-hx+cut,hy),(-hx,hy-cut),(-hx,-hy+cut)]
    front = [(x+px,y+py,z+d*.5) for px,py in outline]
    back = [(x+px,y+py,z-d*.5) for px,py in outline]
    for index in range(1, 7):
        add_triangle(part, material, front[0], front[index], front[index+1])
        add_triangle(part, material, back[0], back[index+1], back[index])
    for index in range(8):
        next_index = (index+1) % 8
        add_quad(part, material, back[index], back[next_index],
                 front[next_index], front[index])


def bolt(x: float, y: float, z: float, face: int) -> None:
    radius, depth, sides = .009, .004, 10
    center = (x, y, z + face * depth)
    ring = [(x + radius*cos(i*2*pi/sides),
             y + radius*sin(i*2*pi/sides), z) for i in range(sides)]
    for index in range(sides):
        nxt = (index+1) % sides
        if face > 0:
            add_triangle(0, 2, center, ring[index], ring[nxt])
        else:
            add_triangle(0, 2, center, ring[nxt], ring[index])
    box(3, (x, y, z + face * (depth+.001)), (.011, .0015, .001))


# The broad feet and two substantial pylons establish the silhouette.
for side in (-1, 1):
    x = side * .43
    chamfered_panel(1, (x, .15, 0), (.29, .29, .52), .035)
    chamfered_panel(1, (x, .86, 0), (.245, 1.47, .40), .032)
    chamfered_panel(1, (x, 1.70, 0), (.29, .20, .46), .035)
    for face in (-1, 1):
        chamfered_panel(0, (x, 1.04, face*.214),
                        (.167, 1.12, .025), .018)
        box(3, (x, 1.07, face*.229), (.044, .83, .009))
        box(5, (x, 1.07, face*.235), (.021, .78, .005), part=1)
        for height in (.17, .46, 1.54, 1.72):
            for dx in (-.075, .075):
                bolt(x+dx, height, face*.236, face)
    # A restrained cyan identification stripe wraps each pylon near its foot.
    for face in (-1, 1):
        box(5, (x, .38, face*.207), (.236, .012, .005), part=2)

# Braces and screen housing are all physically attached. The center remains
# open above the low brace and below the screen, as in the approved concept.
box(1, (0, 1.695, 0), (.77, .115, .20))
box(2, (0, 1.755, 0), (.73, .032, .24))
box(1, (0, .285, 0), (.77, .13, .20))
for side in (-1, 1):
    box(2, (side*.342, 1.14, 0), (.13, .10, .41))
    box(2, (side*.342, .285, 0), (.12, .16, .235))
    bolt(side*.357, 1.14, .215, 1)
    bolt(side*.357, 1.14, -.215, -1)

chamfered_panel(0, (0, 1.12, 0), (.75, .57, .47), .065)
chamfered_panel(2, (0, 1.12, 0), (.715, .535, .49), .058)
for face in (-1, 1):
    chamfered_panel(3, (0, 1.165, face*.253),
                    (.625, .365, .014), .045)
    chamfered_panel(4, (0, 1.165, face*.263),
                    (.545, .295, .009), .018)
    chamfered_panel(1, (0, .827, face*.25),
                    (.325, .115, .075), .022)
    chamfered_panel(2, (0, .833, face*.292),
                    (.192, .053, .008), .010)
    box(5, (0, .833, face*.298), (.095, .010, .004), part=2)
    for x in (-.326, .326):
        for y in (.88, 1.36):
            bolt(x, y, face*.257, face)
    # Abstract direction marker is modeled on the screen; mission text will
    # be provided by game UI, rather than baked into this asset.
    box(5, (0, 1.17, face*.270), (.011, .088, .002), part=2)
    box(5, (0, 1.225, face*.270), (.068, .010, .002), part=2)
    for y in (1.105, 1.077, 1.049):
        box(5, (0, y, face*.270), (.014, .006, .002), part=2)

for x in (-.065, .065):
    box(3, (x, .545, 0), (.032, .37, .045))
    for height in (.45, .52, .59, .66):
        box(2, (x, height, 0), (.041, .010, .052))


def export_runtime(path: Path) -> None:
    with path.open("wb") as stream:
        stream.write(b"ISEMESH1")
        stream.write(struct.pack("<I", len(groups)))
        for (part, material), (vertices, indices) in sorted(groups.items()):
            stream.write(struct.pack("<II3fII", part, material, 0.0, 0.0, 0.0,
                                     len(vertices), len(indices)))
            for vertex in vertices:
                stream.write(struct.pack("<8f", *vertex))
            for index in indices:
                stream.write(struct.pack("<I", index))


def export_obj(path: Path) -> None:
    with path.open("w", encoding="utf-8") as stream:
        stream.write("# Iron Seed portal terminal A; metres, Y up, Z front\n")
        stream.write("mtllib terminal_a.mtl\n")
        offset = 0
        for (part, material), (vertices, indices) in sorted(groups.items()):
            stream.write(f"g part_{part}_material_{material}\n"
                         f"usemtl material_{material}\n")
            for vertex in vertices:
                stream.write("v {:.6f} {:.6f} {:.6f}\n".format(*vertex[:3]))
                stream.write("vt {:.6f} {:.6f}\n".format(*vertex[6:]))
                stream.write("vn {:.6f} {:.6f} {:.6f}\n".format(*vertex[3:6]))
            for index in range(0, len(indices), 3):
                face = [offset + indices[index+i] + 1 for i in range(3)]
                stream.write("f " + " ".join(f"{v}/{v}/{v}" for v in face) + "\n")
            offset += len(vertices)
    palette = ((.58,.56,.51),(.11,.13,.15),(.43,.47,.48),
               (.025,.035,.04),(.01,.04,.055),(.38,.82,.86))
    with path.with_suffix(".mtl").open("w", encoding="utf-8") as stream:
        for index, (r,g,b) in enumerate(palette):
            stream.write(f"newmtl material_{index}\nKd {r} {g} {b}\n"
                         "Ks 0.35 0.35 0.35\nNs 60\n\n")


runtime = OUT / "terminal_a.isemesh"
export_runtime(runtime)
export_obj(OUT / "terminal_a.obj")
triangle_count = sum(len(indices)//3 for _, indices in groups.values())
assert triangle_count < 6000
print(f"TERMINAL_A_TRIANGLES={triangle_count} SUBMESHES={len(groups)} BYTES={runtime.stat().st_size}")

"""Export an edited player weapon .blend to the game's ISEMESH1 format.

Run from the repository root, without rebuilding or saving the Blender source:
    blender -b assets/visual/models/player_weapons/pistol/player_pistol.blend \
        --python tools/visual/export_player_weapon.py -- \
        --weapon pistol --output assets/visual/models/player_weapons/pistol/player_pistol.isemesh
"""

from __future__ import annotations

import argparse
from collections import defaultdict
import math
import os
from pathlib import Path
import re
import struct
import sys

import bpy


PARTS = {
    "pistol": {0, 1, 2, 9},
    "rifle": {0, 1, 2, 9},
    "knife": {0, 9},
    "sniper": {0, 1, 2, 9},
    "shotgun": {0, 2, 9},
    "smg": {0, 1, 2, 9},
    "grenade": {0, 1},
}


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--weapon", choices=PARTS, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])


def material_id(material):
    if material is None:
        raise ValueError("mesh face has no material")
    match = re.match(r"^([0-5])\s*\|", material.name)
    if match is None:
        raise ValueError(f"material {material.name!r} has no 0..5 ID prefix")
    return int(match.group(1))


def export(weapon: str, output: Path):
    source = Path(bpy.data.filepath)
    if not source.is_file() or source.name != f"player_{weapon}.blend":
        raise ValueError(f"expected the saved player_{weapon}.blend, got {source}")
    if output.suffix != ".isemesh" or output.resolve() == source.resolve():
        raise ValueError("output must be a separate .isemesh file")

    property_name = "grenade_part" if weapon == "grenade" else "weapon_part"
    objects = sorted((obj for obj in bpy.context.scene.objects if obj.type == "MESH"),
                     key=lambda obj: obj.name)
    if not objects:
        raise ValueError("the Blender scene has no mesh objects")

    groups = defaultdict(lambda: {"vertices": [], "indices": [], "lookup": {}})
    depsgraph = bpy.context.evaluated_depsgraph_get()
    depsgraph.update()
    for obj in objects:
        if property_name not in obj:
            raise ValueError(f"{obj.name!r} has no {property_name} property")
        part = int(obj[property_name])
        if part not in PARTS[weapon]:
            raise ValueError(f"{obj.name!r} has unsupported part {part}")
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        try:
            mesh.calc_loop_triangles()
            normal_matrix = evaluated.matrix_world.to_3x3().inverted().transposed()
            uv_layer = mesh.uv_layers.active
            for triangle in mesh.loop_triangles:
                slot = triangle.material_index
                if slot >= len(obj.material_slots):
                    raise ValueError(f"{obj.name!r} triangle has invalid material slot {slot}")
                material = material_id(obj.material_slots[slot].material)
                group = groups[(part, material)]
                for loop_id in triangle.loops:
                    loop = mesh.loops[loop_id]
                    position = evaluated.matrix_world @ mesh.vertices[loop.vertex_index].co
                    normal = (normal_matrix @ mesh.corner_normals[loop_id].vector).normalized()
                    if uv_layer is not None:
                        uv = uv_layer.data[loop_id].uv
                        texcoord = (uv.x, uv.y)
                    else:
                        texcoord = (position.x + position.z, position.y + position.z)
                    vertex = (position.x, position.z, -position.y,
                              normal.x, normal.z, -normal.y, *texcoord)
                    if not all(math.isfinite(value) for value in vertex):
                        raise ValueError(f"{obj.name!r} has non-finite geometry")
                    key = tuple(round(float(value), 5) for value in vertex)
                    index = group["lookup"].get(key)
                    if index is None:
                        index = len(group["vertices"])
                        group["lookup"][key] = index
                        group["vertices"].append(vertex)
                    group["indices"].append(index)
        finally:
            evaluated.to_mesh_clear()

    if not groups or len(groups) > 256:
        raise ValueError("invalid number of material and part groups")
    used_parts = {part for part, _ in groups}
    if used_parts != PARTS[weapon]:
        raise ValueError(f"missing expected rigid parts: {PARTS[weapon] - used_parts}")
    triangles = sum(len(group["indices"]) // 3 for group in groups.values())
    if triangles == 0 or triangles > 50_000:
        raise ValueError(f"unexpected triangle count: {triangles}")

    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    try:
        with temporary.open("wb") as file:
            file.write(b"ISEMESH1")
            file.write(struct.pack("<I", len(groups)))
            for (part, material), group in sorted(groups.items()):
                file.write(struct.pack("<II3fII", part, material, 0.0, 0.0, 0.0,
                                       len(group["vertices"]), len(group["indices"])))
                for vertex in group["vertices"]:
                    file.write(struct.pack("<8f", *vertex))
                for index in group["indices"]:
                    file.write(struct.pack("<I", index))
        os.replace(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)
    print(f"WEAPON_EXPORT={output} OBJECTS={len(objects)} GROUPS={len(groups)} "
          f"TRIANGLES={triangles} PARTS={sorted(used_parts)}")


if __name__ == "__main__":
    args = parse_args()
    export(args.weapon, args.output)

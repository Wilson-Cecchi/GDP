"""Export the saved tactical Player .blend to ISPLYR04 preview geometry.

Run from the repository root:
  blender -b -t 2 --python-exit-code 1 \
    assets/visual/models/player_exosuit/player_exosuit.blend \
    --python tools/blender/export_player_exosuit.py

The source .blend is read only. This exporter writes only the .isplayer file.
"""

import math
import struct
from pathlib import Path

import bpy
from mathutils import Matrix


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets/visual/models/player_exosuit/player_exosuit.blend"
DEST = ROOT / "assets/visual/models/player_exosuit/player_exosuit.isplayer"
if Path(bpy.data.filepath).resolve() != SOURCE.resolve():
    raise RuntimeError("Open player_exosuit.blend before exporting")
RIG = bpy.data.objects["Player_Rig"]
MAGIC = b"ISPLYR04"
MATERIALS = (
    "M01_Graphite_Cloth", "M02_Stitched_Trim", "M03_Reinforcement",
    "M04_Belt_Leather", "M05_Blackened_Metal", "M06_Rubber",
    "M07_Worn_Orange", "M08_Cyan_Visor",
)
BONES = (
    "root", "pelvis", "spine", "chest", "neck",
    "clavicle.L", "upper_arm.L", "forearm.L", "hand.L",
    "thigh.L", "shin.L", "foot.L",
    "clavicle.R", "upper_arm.R", "forearm.R", "hand.R",
    "thigh.R", "shin.R", "foot.R",
)
CLIPS = ("Idle", "Walk", "Jog", "Crouch", "Jump", "Draw_Primary_Back")
PARTS = ("torso", "legs", "arm.L", "arm.R", "world-only")
TORSO_BONES = {"pelvis", "spine", "chest"}
LEG_BONES = {f"{stem}.{side}" for side in ("L", "R")
             for stem in ("thigh", "shin", "foot")}
ARM_BONES = {side: {f"{stem}.{side}" for stem in
                         ("clavicle", "upper_arm", "forearm", "hand")}
             for side in ("L", "R")}
HELMET_PREFIXES = ("Helmet_", "Visor_")
# The FPS camera looks down through these large upper-chest and shoulder
# shells. Keep them in the world silhouette but outside the first-person
# groups; lower torso, sleeves, forearms, gloves, and leg pieces remain.
WORLD_ONLY_ARMOR = {
    *(f"Chest_Outer_Bib.{side}" for side in ("L", "R")),
    *(f"Shoulder_Pad_Base.{side}" for side in ("L", "R")),
    *(f"Shoulder_Orange_Shell.{side}" for side in ("L", "R")),
    *(f"Shoulder_Front_Orange.{side}" for side in ("L", "R")),
}

# These objects stay in the editable source but obstruct a first-person camera
# near the neck. The previous left grenade placeholder also conflicts with
# the newer visual sheet's right-front attachment. Keep the socket name.
EXCLUDED = {
    "Jacket_CenterZip": "M05_Blackened_Metal",
    "Jacket_Open_Collar": "M03_Reinforcement",
    "Shoulder_Seam_-1": "M02_Stitched_Trim",
    "Shoulder_Seam_1": "M02_Stitched_Trim",
    "Grenade_Clip_Left_Front": "M05_Blackened_Metal",
}

assert len(BONES) == 19 and len(MATERIALS) == 8 and len(CLIPS) == 6
assert RIG.type == "ARMATURE"
assert set(BONES) == set(RIG.data.bones.keys())
assert set(CLIPS) == set(bpy.data.actions.keys())
assert RIG.matrix_world == Matrix.Identity(4), "rig transform must be identity"
for name, expected_material in EXCLUDED.items():
    obj = bpy.data.objects[name]
    assert obj.type == "MESH" and obj.data.materials[0].name == expected_material
for name in MATERIALS:
    assert name in bpy.data.materials

# Blender (x,y,z) -> game (x,z,y). Reflection reverses triangle winding.
C = Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
assert round(C.determinant()) == -1 and C.inverted() == C
BONE_IDS = {name: i for i, name in enumerate(BONES)}
MATERIAL_IDS = {name: i for i, name in enumerate(MATERIALS)}


def game_vec(vector):
    return (float(vector.x), float(vector.z), float(vector.y))


def influences_for(obj, vertex):
    influences = []
    for item in vertex.groups:
        if item.weight <= 1e-6:
            continue
        name = obj.vertex_groups[item.group].name
        assert name in BONE_IDS, (obj.name, name)
        influences.append((BONE_IDS[name], float(item.weight)))
    influences.sort(key=lambda value: (-value[1], value[0]))
    assert 1 <= len(influences) <= 2, (obj.name, vertex.index, influences)
    total = sum(weight for _, weight in influences)
    assert math.isfinite(total) and total > 0
    if len(influences) == 1:
        return influences[0][0], influences[0][0], 1.0
    return influences[0][0], influences[1][0], influences[0][1]/total


def object_part(obj):
    used = {obj.vertex_groups[item.group].name
            for vertex in obj.data.vertices for item in vertex.groups
            if item.weight > 1e-6}
    assert used, obj.name
    if obj.name.startswith(HELMET_PREFIXES):
        assert used == {"neck"}, (obj.name, used)
        return 4
    if obj.name in WORLD_ONLY_ARMOR:
        assert used == {"chest"} or used <= ARM_BONES["L"] or used <= ARM_BONES["R"], (obj.name, used)
        return 4
    assert "neck" not in used, (obj.name, used)
    if used <= TORSO_BONES:
        return 0
    if used <= LEG_BONES:
        return 1
    if used <= ARM_BONES["L"]:
        return 2
    if used <= ARM_BONES["R"]:
        return 3
    raise AssertionError((obj.name, used))


source_meshes = sorted((obj for obj in bpy.data.objects
                        if obj.type == "MESH" and obj.name not in EXCLUDED),
                       key=lambda obj: obj.name)
objects_by_part = {part: [] for part in range(len(PARTS))}
for obj in source_meshes:
    assert len(obj.data.materials) == 1 and obj.data.materials[0].name in MATERIAL_IDS, obj.name
    assert obj.parent == RIG, obj.name
    assert any(mod.type == "ARMATURE" and mod.object == RIG for mod in obj.modifiers), obj.name
    objects_by_part[object_part(obj)].append(obj)
assert sum(map(len, objects_by_part.values())) == len(source_meshes)
assert all(objects_by_part[part] for part in range(len(PARTS)))
assert len(objects_by_part[4]) == 26 + len(WORLD_ONLY_ARMOR)
assert WORLD_ONLY_ARMOR <= {obj.name for obj in objects_by_part[4]}
assert "Grenade_Right_Front_Clip" in {obj.name for obj in objects_by_part[0]}


def flatten_material(part_id, material_id):
    vertices = []
    indices = []
    for obj in objects_by_part[part_id]:
        if MATERIAL_IDS[obj.data.materials[0].name] != material_id:
            continue
        mesh = obj.data
        mesh.calc_loop_triangles()
        normal_transform = obj.matrix_world.to_3x3().inverted().transposed()
        seen = {}
        for triangle in mesh.loop_triangles:
            for loop_id in (triangle.loops[0], triangle.loops[2], triangle.loops[1]):
                loop = mesh.loops[loop_id]
                source_vertex = mesh.vertices[loop.vertex_index]
                normal = (normal_transform @ mesh.corner_normals[loop_id].vector).normalized()
                key = (obj.name, loop.vertex_index,
                       *(round(float(value), 6) for value in normal))
                index = seen.get(key)
                if index is None:
                    position = obj.matrix_world @ source_vertex.co
                    bone0, bone1, weight0 = influences_for(obj, source_vertex)
                    # Existing GLSL: bone0=floor(uv.x/2), weight0=uv.x-2*bone0,
                    # bone1=int(uv.y). Both fields carry skin data, not albedo UV.
                    encoded = (2.0*bone0+weight0, float(bone1))
                    vertex = (*game_vec(position), *game_vec(normal), *encoded)
                    assert all(math.isfinite(value) for value in vertex)
                    index = len(vertices)
                    vertices.append(vertex)
                    seen[key] = index
                indices.append(index)
    assert len(indices) % 3 == 0
    assert bool(vertices) == bool(indices)
    assert len(vertices) < 500_000 and len(indices) < 1_500_000
    return vertices, indices


def write_matrix(payload, matrix):
    assert all(math.isfinite(matrix[row][col])
               for row in range(4) for col in range(4))
    payload.extend(struct.pack("<16f", *(matrix[row][col]
                                         for col in range(4) for row in range(4))))


def clip_frames(name):
    action = bpy.data.actions[name]
    start = int(round(action.frame_range[0]))
    end = int(round(action.frame_range[1]))
    assert start >= 0 and end >= start and end-start < 300
    assert abs(action.frame_range[0]-start) < 1e-4
    assert abs(action.frame_range[1]-end) < 1e-4
    RIG.animation_data.action = action
    frames = []
    for frame in range(start, end+1):
        bpy.context.scene.frame_set(frame)
        bpy.context.view_layer.update()
        skin = []
        for bone_name in BONES:
            bone = RIG.data.bones[bone_name]
            pose = RIG.pose.bones[bone_name]
            skin_blender = pose.matrix @ bone.matrix_local.inverted()
            skin.append(C @ skin_blender @ C)
        frames.append(skin)
    return start, frames


RIG.animation_data.action = bpy.data.actions["Idle"]
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
groups = [flatten_material(part, material)
          for part in range(len(PARTS)) for material in range(len(MATERIALS))]
payload = bytearray(MAGIC)
payload.extend(struct.pack("<3I", len(BONES), len(groups), len(CLIPS)))
for group_id, (vertices, indices) in enumerate(groups):
    payload.extend(struct.pack("<3I", group_id % len(MATERIALS),
                               len(vertices), len(indices)))
    for vertex in vertices:
        payload.extend(struct.pack("<8f", *vertex))
    payload.extend(struct.pack(f"<{len(indices)}I", *indices))
for name in CLIPS:
    start, frames = clip_frames(name)
    ascii_name = name.encode("ascii")
    assert len(ascii_name) < 32
    payload.extend(ascii_name.ljust(32, b"\0"))
    payload.extend(struct.pack("<2I", len(frames), start))
    for frame in frames:
        for matrix in frame:
            write_matrix(payload, matrix)

assert len(payload) < 32*1024*1024
temporary = DEST.with_suffix(".isplayer.tmp")
temporary.write_bytes(payload)
temporary.replace(DEST)
print("ISPLAYER04_EXPORT_OK", DEST, "bytes", len(payload),
      "excluded", sorted(EXCLUDED),
      "parts", [(PARTS[part], len(objects_by_part[part])) for part in range(len(PARTS))],
      "meshes", [(len(v), len(i)) for v, i in groups],
      "bones", len(BONES), "clips", len(CLIPS))

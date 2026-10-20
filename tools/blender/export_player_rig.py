"""Export the saved Player .blend to the small ISPLYR01 runtime-preview format.

Run from the repository root:
  blender -b --python-exit-code 1 assets/visual/models/player_rig.blend \
    --python tools/blender/export_player_rig.py

This script reads the .blend. It never saves or mutates the source file.
"""

import math
import struct
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / "assets/visual/models/player_rig/player_rig.isplayer"
RIG = bpy.data.objects["Player_Rig"]
MAGIC = b"ISPLYR01"
MATERIALS = (
    "M01_Graphite_Cloth", "M02_Stitched_Trim", "M03_Reinforcement",
    "M04_Belt_Leather", "M05_Blackened_Metal", "M06_Rubber",
)
BONES = (
    "root", "pelvis", "spine", "chest", "neck",
    "clavicle.L", "upper_arm.L", "forearm.L", "hand.L",
    "thigh.L", "shin.L", "foot.L",
    "clavicle.R", "upper_arm.R", "forearm.R", "hand.R",
    "thigh.R", "shin.R", "foot.R",
)
CLIPS = ("Idle", "Walk", "Jog", "Crouch", "Jump", "Draw_Primary_Back")
# The Blender source retains its center zipper. In first person, that thin
# vertical detail projects directly in front of the camera as a bright stick.
# Omit only this object from the opt-in runtime preview export.
PREVIEW_EXCLUDED_OBJECT = "Jacket_CenterZip"
assert len(BONES) == 19 and len(MATERIALS) == 6 and len(CLIPS) == 6
assert RIG.type == "ARMATURE"
assert set(BONES) == set(RIG.data.bones.keys())
assert set(CLIPS).issubset(bpy.data.actions.keys())
assert RIG.matrix_world == Matrix.Identity(4), "rig transform must remain identity"
assert PREVIEW_EXCLUDED_OBJECT in bpy.data.objects
assert bpy.data.objects[PREVIEW_EXCLUDED_OBJECT].type == "MESH"
assert bpy.data.objects[PREVIEW_EXCLUDED_OBJECT].data.materials[0].name == MATERIALS[4]

# Parent-approved model-space axis contract: Blender (x,y,z) -> game (x,z,y).
# C is a reflection (det=-1), so triangle winding must be reversed on export.
C = Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
assert round(C.determinant()) == -1
assert C.inverted() == C


def game_vec(vector):
    return (float(vector.x), float(vector.z), float(vector.y))


def weights_for(obj, vertex):
    influences = []
    for item in vertex.groups:
        if item.weight <= 1e-6:
            continue
        name = obj.vertex_groups[item.group].name
        assert name in BONES, (obj.name, name)
        influences.append((BONES.index(name), float(item.weight)))
    influences.sort(key=lambda value: (-value[1], value[0]))
    assert 1 <= len(influences) <= 2, (obj.name, vertex.index, influences)
    total = sum(weight for _, weight in influences)
    assert math.isfinite(total) and total > 0
    if len(influences) == 1:
        return influences[0][0], influences[0][0], 1.0
    return influences[0][0], influences[1][0], influences[0][1] / total


def flatten_material(material_id):
    vertices = []
    indices = []
    for obj in sorted((o for o in bpy.data.objects if o.type == "MESH"), key=lambda o: o.name):
        if obj.name == PREVIEW_EXCLUDED_OBJECT:
            continue
        assert len(obj.data.materials) == 1, obj.name
        if obj.data.materials[0].name != MATERIALS[material_id]:
            continue
        assert obj.parent == RIG, obj.name
        assert any(mod.type == "ARMATURE" and mod.object == RIG for mod in obj.modifiers), obj.name
        mesh = obj.data
        mesh.calc_loop_triangles()
        normal_transform = obj.matrix_world.to_3x3().inverted().transposed()
        seen = {}
        for triangle in mesh.loop_triangles:
            assert len(triangle.loops) == 3
            # Reversal compensates the reflected C transform, preserving the
            # source-facing side and its shading normal in game space.
            for loop_index in (triangle.loops[0], triangle.loops[2], triangle.loops[1]):
                loop = mesh.loops[loop_index]
                source_vertex = mesh.vertices[loop.vertex_index]
                normal = (normal_transform @ mesh.corner_normals[loop_index].vector).normalized()
                key = (obj.name, loop.vertex_index, *(round(float(x), 6) for x in normal))
                index = seen.get(key)
                if index is None:
                    position = obj.matrix_world @ source_vertex.co
                    bone0, bone1, weight0 = weights_for(obj, source_vertex)
                    # Existing GLSL decodes joint=floor(uv.x/2),
                    # weight=uv.x-2*joint; second joint=int(uv.y).
                    encoded = (2.0*bone0+weight0, float(bone1))
                    vertex = (*game_vec(position), *game_vec(normal), *encoded)
                    assert all(math.isfinite(value) for value in vertex)
                    index = len(vertices)
                    vertices.append(vertex)
                    seen[key] = index
                indices.append(index)
    assert vertices and indices and len(indices) % 3 == 0
    assert len(vertices) < 500_000 and len(indices) < 1_500_000
    return vertices, indices


def write_matrix(out, matrix):
    assert all(math.isfinite(matrix[row][col]) for row in range(4) for col in range(4))
    # wkc::core::Mat4 and OpenGL uniforms are column-major.
    out.extend(struct.pack("<16f", *(matrix[row][col] for col in range(4) for row in range(4))))


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
        frame_matrices = []
        for name_of_bone in BONES:
            bone = RIG.data.bones[name_of_bone]
            posed = RIG.pose.bones[name_of_bone]
            skin_blender = posed.matrix @ bone.matrix_local.inverted()
            skin_game = C @ skin_blender @ C
            frame_matrices.append(skin_game)
        frames.append(frame_matrices)
    return start, frames


# Export the undeformed bind mesh, independent of the active saved action.
RIG.animation_data.action = bpy.data.actions["Idle"]
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
groups = [flatten_material(i) for i in range(len(MATERIALS))]
payload = bytearray(MAGIC)
payload.extend(struct.pack("<3I", len(BONES), len(groups), len(CLIPS)))
for material_id, (vertices, indices) in enumerate(groups):
    payload.extend(struct.pack("<3I", material_id, len(vertices), len(indices)))
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
print("ISPLAYER_EXPORT_OK", DEST, "bytes", len(payload),
      "meshes", [(len(v), len(i)) for v, i in groups],
      "bones", len(BONES), "clips", len(CLIPS))

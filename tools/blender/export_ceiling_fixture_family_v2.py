"""Export the approved Blender ceiling-fixture family to ISEMESH1.

Run from the repository root:
  blender -b -t 2 --python-exit-code 1 \
    visual/proposals/ceiling_fixture_family_v2/ceiling_fixture_family_v2.blend \
    --python tools/blender/export_ceiling_fixture_family_v2.py

Reads only the nine Used/ON fixtures in scene "Family board". The source .blend
is never saved. Game coordinates are (Blender X, Blender Z, -Blender Y) in
meters, relative to each housing center; the runtime places the mesh at the
light position with its own +0.095 m vertical offset.
"""

import math
import struct
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "visual/proposals/ceiling_fixture_family_v2/ceiling_fixture_family_v2.blend"
OUT = ROOT / "assets/visual/models/ceiling_fixture_family_v2"
if Path(bpy.data.filepath).resolve() != SOURCE.resolve():
    raise RuntimeError(f"Open the approved Blender source first: {SOURCE}")

SCENE = bpy.data.scenes.get("Family board")
if SCENE is None:
    raise RuntimeError("Missing Family board scene")

# IDs are stable semantic groups for the renderer; 5 is reserved by ISEMESH1.
PAINT, METAL, GASKET, FASTENER, LENS = range(5)
ATLAS_U = {
    PAINT: (0.02, 0.48),
    METAL: (0.52, 0.66),
    GASKET: (0.69, 0.74),
    FASTENER: (0.52, 0.66),
    LENS: (0.77, 0.98),
}
ATLAS_V = (0.02, 0.98)
THEMES = {
    "Industrial": "industrial.isemesh",
    "Storage": "storage.isemesh",
    "Maintenance": "maintenance.isemesh",
    "Laboratory": "laboratory.isemesh",
    "Cafeteria": "cafeteria.isemesh",
    "Security": "security.isemesh",
    "Office": "office.isemesh",
    "Boss / Arena": "boss_arena.isemesh",
    "Central Hub": "central_hub.isemesh",
}


def material_id(name):
    if name.endswith("/paint"):
        return PAINT
    if name.endswith("/edge metal"):
        return METAL
    if name == "Fixture/common black gasket" or name == "Fixture/diffuser dust":
        return GASKET
    if name == "Fixture/common fasteners":
        return FASTENER
    if name.endswith("/separate emissive lens"):
        return LENS
    raise ValueError(f"Unmapped fixture material: {name}")


def game_vec(vector):
    return float(vector.x), float(vector.z), -float(vector.y)


def unit_coord(value, lo, hi):
    return 0.5 if hi - lo < 1e-8 else min(1.0, max(0.0, (value-lo)/(hi-lo)))


def atlas_uv(position, face_normal, mesh_min, mesh_max, group):
    # Project on the face's two dominant surface axes. This does not rely on
    # non-existent source UV layers, and never samples another atlas strip.
    axis = max(range(3), key=lambda a: abs(face_normal[a]))
    u_axis, v_axis = ((1,2) if axis == 0 else (0,2) if axis == 1 else (0,1))
    u = unit_coord(position[u_axis], mesh_min[u_axis], mesh_max[u_axis])
    v = unit_coord(position[v_axis], mesh_min[v_axis], mesh_max[v_axis])
    u0, u1 = ATLAS_U[group]
    v0, v1 = ATLAS_V
    return u0 + (u1-u0)*u, v0 + (v1-v0)*v


def fixture_geometry(housing, depsgraph):
    prefix = housing.name.split("/", 1)[0] + "/"
    pieces = sorted((obj for obj in SCENE.objects if obj.type == "MESH"
                     and obj.get("fixture_part") and obj.name.startswith(prefix)),
                    key=lambda obj: obj.name)
    if not pieces or housing not in pieces:
        raise RuntimeError(f"No fixture pieces for {housing.name}")
    if not any("/Diffuser lens" in obj.name for obj in pieces):
        raise RuntimeError(f"Missing separate lens for {housing.name}")
    origin = housing.matrix_world.translation.copy()
    groups = {group: ([], []) for group in range(5)}
    for obj in pieces:
        if len(obj.material_slots) != 1 or obj.material_slots[0].material is None:
            raise RuntimeError(f"Expected one material on {obj.name}")
        group = material_id(obj.material_slots[0].material.name)
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
        if mesh is None:
            raise RuntimeError(f"Could not evaluate {obj.name}")
        try:
            mesh.calc_loop_triangles()
            if not mesh.loop_triangles:
                raise RuntimeError(f"No triangles on {obj.name}")
            minimum = tuple(min(vertex.co[a] for vertex in mesh.vertices)
                            for a in range(3))
            maximum = tuple(max(vertex.co[a] for vertex in mesh.vertices)
                            for a in range(3))
            normal_matrix = obj.matrix_world.to_3x3().inverted().transposed()
            vertices, indices = groups[group]
            for triangle in mesh.loop_triangles:
                for loop_index in triangle.loops:
                    loop = mesh.loops[loop_index]
                    local_pos = mesh.vertices[loop.vertex_index].co
                    world_pos = obj.matrix_world @ local_pos
                    relative_pos = world_pos-origin
                    local_normal = mesh.corner_normals[loop_index].vector
                    normal = (normal_matrix @ local_normal).normalized()
                    uv = atlas_uv(local_pos, triangle.normal, minimum, maximum, group)
                    vertex = (*game_vec(relative_pos), *game_vec(normal), *uv)
                    if not all(math.isfinite(value) for value in vertex):
                        raise RuntimeError(f"Non-finite vertex on {obj.name}")
                    indices.append(len(vertices))
                    vertices.append(vertex)
        finally:
            evaluated.to_mesh_clear()
    if not groups[LENS][0]:
        raise RuntimeError(f"Lens submesh missing for {housing.name}")
    return {group: values for group, values in groups.items() if values[0]}


def save_file(path, groups):
    payload = bytearray(b"ISEMESH1")
    payload += struct.pack("<I", len(groups))
    for group, (vertices, indices) in sorted(groups.items()):
        payload += struct.pack("<II3fII", 0, group, 0.0, 0.0, 0.0,
                               len(vertices), len(indices))
        for vertex in vertices:
            payload += struct.pack("<8f", *vertex)
        payload += struct.pack(f"<{len(indices)}I", *indices)
    if len(payload) > 32*1024*1024:
        raise RuntimeError(f"ISEMESH1 over 32 MiB: {path}")
    path.write_bytes(payload)


def validate_file(path):
    data = path.read_bytes()
    if data[:8] != b"ISEMESH1":
        raise RuntimeError(f"Bad magic: {path}")
    offset = 8
    count, = struct.unpack_from("<I", data, offset)
    offset += 4
    if not (2 <= count <= 5):
        raise RuntimeError(f"Bad submesh count in {path}: {count}")
    found = set()
    positions = []
    total_triangles = 0
    for _ in range(count):
        part, group, px, py, pz, vertex_count, index_count = struct.unpack_from(
            "<II3fII", data, offset)
        offset += 28
        if part != 0 or group not in ATLAS_U or group in found:
            raise RuntimeError(f"Bad part/material in {path}: {part}/{group}")
        if any(abs(value) > 1e-6 for value in (px,py,pz)):
            raise RuntimeError(f"Nonzero pivot in {path}")
        if not (0 < vertex_count < 750_000) or not (0 < index_count <= 2_250_000):
            raise RuntimeError(f"Bad counts in {path}: {vertex_count}/{index_count}")
        if index_count % 3:
            raise RuntimeError(f"Non-triangle indices in {path}")
        vertices = [struct.unpack_from("<8f", data, offset+32*i)
                    for i in range(vertex_count)]
        offset += vertex_count*32
        indices = struct.unpack_from(f"<{index_count}I", data, offset)
        offset += index_count*4
        if max(indices) >= vertex_count:
            raise RuntimeError(f"Out-of-range index in {path}")
        u0,u1 = ATLAS_U[group]
        for vertex in vertices:
            if not all(math.isfinite(value) for value in vertex):
                raise RuntimeError(f"Non-finite vertex in {path}")
            normal_length = math.sqrt(sum(n*n for n in vertex[3:6]))
            if not 0.98 <= normal_length <= 1.02:
                raise RuntimeError(f"Bad normal in {path}")
            if not (u0-1e-5 <= vertex[6] <= u1+1e-5 and
                    ATLAS_V[0]-1e-5 <= vertex[7] <= ATLAS_V[1]+1e-5):
                raise RuntimeError(f"UV outside atlas strip in {path}: {group} {vertex[6:8]}")
            positions.append(vertex[:3])
        total_triangles += index_count//3
        found.add(group)
    if offset != len(data) or LENS not in found or PAINT not in found:
        raise RuntimeError(f"Trailing bytes or missing paint/lens in {path}")
    bounds = [(min(v[a] for v in positions), max(v[a] for v in positions))
              for a in range(3)]
    spans = [hi-lo for lo,hi in bounds]
    suspended = path.name == "security.isemesh"
    expected_height = (0.42, 0.48) if suspended else (0.12, 0.25)
    if not (1.18 <= spans[0] <= 1.24
            and expected_height[0] <= spans[1] <= expected_height[1]
            and 0.27 <= spans[2] <= 0.34):
        raise RuntimeError(f"Unexpected bounds in {path}: {bounds}")
    centers = [(lo+hi)/2 for lo,hi in bounds]
    if (abs(centers[0]) > 0.04 or abs(centers[2]) > 0.04
            or (not 0.10 <= centers[1] <= 0.16 if suspended
                else abs(centers[1]) > 0.04)):
        raise RuntimeError(f"Fixture not centered on housing in {path}: {bounds}")
    return bounds, total_triangles, len(data), sorted(found)


def main():
    housings = [obj for obj in SCENE.objects if obj.get("fixture_housing")]
    by_theme = {obj.get("room_variant"): obj for obj in housings}
    if len(housings) != 9 or set(by_theme) != set(THEMES):
        raise RuntimeError(f"Expected nine unique family housings: {list(by_theme)}")
    if any(obj.get("material_condition") != "Used" or
           obj.get("emission_state") != "ON" for obj in housings):
        raise RuntimeError("Family board must contain only Used/ON fixtures")
    depsgraph = bpy.context.evaluated_depsgraph_get()
    OUT.mkdir(parents=True, exist_ok=True)
    for theme, filename in THEMES.items():
        path = OUT / filename
        save_file(path, fixture_geometry(by_theme[theme], depsgraph))
        bounds, triangles, size, groups = validate_file(path)
        print(f"VALID {filename}: {triangles} triangles, {size} bytes, "
              f"groups={groups}, bounds={bounds}")
    print("EXPORTED 9 ISEMESH1 fixtures")


main()

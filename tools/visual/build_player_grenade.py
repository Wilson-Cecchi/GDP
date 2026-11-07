"""Build the editable player grenade and its compact ISEMESH1 game export.

Run: blender -b --factory-startup --python tools/visual/build_player_grenade.py
"""

from collections import defaultdict
from pathlib import Path
import math
import struct

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets/visual/models/player_weapons/grenade"
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)


def material(name, color, metallic=0.0, roughness=0.7, emission=0.0):
    result = bpy.data.materials.new(name)
    result.diffuse_color = (*color, 1.0)
    result.use_nodes = True
    shader = result.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1.0)
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Roughness"].default_value = roughness
    if emission:
        shader.inputs["Emission Color"].default_value = (*color, 1.0)
        shader.inputs["Emission Strength"].default_value = emission
    return result


PAINT = material("0 | Worn safety orange", (0.76, 0.25, 0.035), 0.5, 0.46)
GRAPHITE = material("1 | Graphite housing", (0.055, 0.065, 0.074), 0.35, 0.68)
STEEL = material("2 | Exposed steel", (0.38, 0.42, 0.45), 0.8, 0.35)
CYAN = material("3 | Cyan status lamp", (0.025, 0.8, 0.95), 0.05, 0.25, 2.0)
COATED = material("4 | Dark coated steel", (0.14, 0.16, 0.18), 0.72, 0.48)
RECESS = material("5 | Recess and fastener", (0.018, 0.022, 0.025), 0.2, 0.9)
MATERIAL_IDS = {PAINT: 0, GRAPHITE: 1, STEEL: 2, CYAN: 3, COATED: 4, RECESS: 5}
paint_image = bpy.data.images.load(str(ROOT /
    "assets/visual/models/worker_reference/worker_paint_albedo.png"))
paint_node = PAINT.node_tree.nodes.new("ShaderNodeTexImage")
paint_node.image = paint_image
PAINT.node_tree.links.new(paint_node.outputs["Color"],
    PAINT.node_tree.nodes.get("Principled BSDF").inputs["Base Color"])
objects = []


def register(obj, name, mat, part=0, bevel=0.0):
    obj.name = name
    obj.data.materials.append(mat)
    obj["grenade_part"] = part
    if bevel:
        modifier = obj.modifiers.new("soft machined edge", "BEVEL")
        modifier.width = bevel
        modifier.segments = 1
        obj.modifiers.new("weighted face normals", "WEIGHTED_NORMAL")
    objects.append(obj)
    return obj


def cube(name, loc, size, mat, bevel=0.0, part=0, rotation=None):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    obj = bpy.context.object
    obj.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if rotation:
        obj.rotation_euler = rotation
    return register(obj, name, mat, part, bevel)


def cylinder(name, loc, radius, depth, mat, vertices=24, bevel=0.0, part=0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=depth, location=loc)
    return register(bpy.context.object, name, mat, part, bevel)


def torus(name, loc, major, minor, mat, part=0, rotation=None):
    bpy.ops.mesh.primitive_torus_add(major_segments=16, minor_segments=4,
        location=loc, major_radius=major, minor_radius=minor)
    obj = bpy.context.object
    if rotation:
        obj.rotation_euler = rotation
    return register(obj, name, mat, part)


def spherical_panel(band, index):
    # Segmented armor has physical thickness and exposed graphite seams.
    around = 8
    longitude0 = math.tau * (index + 0.085) / around
    longitude1 = math.tau * (index + 0.915) / around
    latitude0, latitude1 = ((0.48, 1.51) if band == 0 else (1.64, 2.66))
    rows, cols = 3, 3
    vertices = []
    for radius in (0.525, 0.492):
        for row in range(rows + 1):
            latitude = latitude0 + (latitude1 - latitude0) * row / rows
            for col in range(cols + 1):
                longitude = longitude0 + (longitude1 - longitude0) * col / cols
                vertices.append((radius * math.sin(latitude) * math.cos(longitude),
                                 radius * math.sin(latitude) * math.sin(longitude),
                                 radius * math.cos(latitude)))
    plane = (rows + 1) * (cols + 1)
    faces = []
    for row in range(rows):
        for col in range(cols):
            a = row * (cols + 1) + col
            b = a + cols + 1
            faces.append((a, b, b + 1, a + 1))
            faces.append((a + plane + 1, b + plane + 1, b + plane, a + plane))
    for row in range(rows):
        a = row * (cols + 1)
        b = (row + 1) * (cols + 1)
        faces.extend(((a + plane, b + plane, b, a),
                      (a + cols, b + cols, b + cols + plane, a + cols + plane)))
    for col in range(cols):
        faces.extend(((col, col + 1, col + 1 + plane, col + plane),
                      (rows * (cols + 1) + col + plane,
                       rows * (cols + 1) + col + 1 + plane,
                       rows * (cols + 1) + col + 1,
                       rows * (cols + 1) + col)))
    mesh = bpy.data.meshes.new("pressed armor panel")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(f"armor panel {band}-{index}", mesh)
    bpy.context.collection.objects.link(obj)
    uv = mesh.uv_layers.new(name="paint grain")
    for polygon in mesh.polygons:
        for loop_id in polygon.loop_indices:
            point = mesh.vertices[mesh.loops[loop_id].vertex_index].co
            uv.data[loop_id].uv = (point.x * 1.7 + point.z,
                                    point.y * 1.7 + point.z)
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    paint = ((index + band) % 4 != 0)
    register(obj, obj.name, PAINT if paint else COATED, bevel=0.006)
    # Dark screw socket and raised head at one corner of each panel.
    latitude = latitude0 + 0.10
    longitude = longitude0 + 0.12
    center = Vector((math.sin(latitude) * math.cos(longitude),
                     math.sin(latitude) * math.sin(longitude),
                     math.cos(latitude))) * 0.535
    socket = cylinder(f"panel {band}-{index} screw socket", center, 0.018,
                      0.006, RECESS, vertices=10)
    socket.rotation_euler = center.to_track_quat("Z", "Y").to_euler()
    head = cylinder(f"panel {band}-{index} screw", center * 1.008, 0.009,
                    0.009, STEEL, vertices=10)
    head.rotation_euler = socket.rotation_euler


bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=12, radius=0.487)
register(bpy.context.object, "rounded graphite inner shell", GRAPHITE)
for band in range(2):
    for index in range(8):
        spherical_panel(band, index)

# Raised latitude rings, rib seams, and electrical slots show depth between panels.
for z, radius in ((0.035, 0.502), (-0.035, 0.502), (0.438, 0.24), (-0.438, 0.24)):
    torus(f"machined ring z={z}", (0, 0, z), radius, 0.013, STEEL)
for index in range(8):
    angle = math.tau * index / 8
    x, y = math.cos(angle), math.sin(angle)
    cube(f"cyan slot recess {index}", (0.503*x, 0.503*y, 0),
         (0.024, 0.048, 0.14), RECESS, 0.007,
         rotation=(0, 0, angle))
    cube(f"cyan slot {index}", (0.52*x, 0.52*y, 0),
         (0.012, 0.019, 0.092), CYAN, 0.004,
         rotation=(0, 0, angle))

# Top fuse housing and a mechanical spoon occupy the grenade's upper rear.
cylinder("top cap steel base", (0, 0, 0.475), 0.255, 0.070, STEEL, bevel=0.013)
cylinder("top black safety collar", (0, 0, 0.515), 0.225, 0.049, COATED, bevel=0.008)
torus("top cyan timing band", (0, 0, 0.538), 0.205, 0.011, CYAN)
cylinder("top head", (0, 0, 0.591), 0.175, 0.099, COATED, bevel=0.016)
cube("latch mechanism", (-0.18, 0.0, 0.612), (0.18, 0.22, 0.11), STEEL, 0.015)
cube("spoon graphite spine", (-0.35, 0, 0.30), (0.060, 0.13, 0.49), GRAPHITE,
     0.015, rotation=(0, -0.15, 0))
cube("spoon orange outer plate", (-0.385, 0, 0.30),
     (0.028, 0.105, 0.44), PAINT, 0.011, rotation=(0, -0.15, 0))
cylinder("pin socket", (-0.18, -0.10, 0.624), 0.048, 0.024, STEEL,
         vertices=16, bevel=0.006)
torus("removable pin ring", (-0.25, -0.13, 0.73), 0.17, 0.014, STEEL,
      part=1, rotation=(math.pi / 2, 0, 0))

# ISEMESH1 groups share the same semantics as the enemy Blender exports.
groups = defaultdict(lambda: {"vertices": [], "indices": [], "lookup": {}})
depsgraph = bpy.context.evaluated_depsgraph_get()
depsgraph.update()
for obj in objects:
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    mesh.calc_loop_triangles()
    normal_matrix = evaluated.matrix_world.to_3x3().inverted().transposed()
    group = groups[(int(obj["grenade_part"]), MATERIAL_IDS[obj.data.materials[0]])]
    for triangle in mesh.loop_triangles:
        for loop_id in triangle.loops:
            loop = mesh.loops[loop_id]
            position = evaluated.matrix_world @ mesh.vertices[loop.vertex_index].co
            normal = (normal_matrix @ mesh.corner_normals[loop_id].vector).normalized()
            if mesh.uv_layers.active is not None:
                uv = mesh.uv_layers.active.data[loop_id].uv
                texcoord = (uv.x, uv.y)
            else:
                texcoord = (position.x + position.z, position.y + position.z)
            vertex = (position.x, position.z, -position.y,
                      normal.x, normal.z, -normal.y, *texcoord)
            key = tuple(round(value, 5) for value in vertex)
            vertex_id = group["lookup"].get(key)
            if vertex_id is None:
                vertex_id = len(group["vertices"])
                group["lookup"][key] = vertex_id
                group["vertices"].append(vertex)
            group["indices"].append(vertex_id)
    evaluated.to_mesh_clear()

asset = OUT / "player_grenade.isemesh"
with asset.open("wb") as file:
    file.write(b"ISEMESH1")
    file.write(struct.pack("<I", len(groups)))
    for (part, mat), data in sorted(groups.items()):
        file.write(struct.pack("<II3fII", part, mat, 0, 0, 0,
                               len(data["vertices"]), len(data["indices"])))
        for vertex in data["vertices"]:
            file.write(struct.pack("<8f", *vertex))
        for index in data["indices"]:
            file.write(struct.pack("<I", index))

bpy.context.preferences.filepaths.save_version = 0
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "player_grenade.blend"))
print(f"GRENADE_EXPORT={asset} GROUPS={len(groups)} TRIANGLES="
      f"{sum(len(group['indices']) for group in groups.values()) // 3}")

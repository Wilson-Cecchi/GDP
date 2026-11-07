"""Build the editable player knife and ISEMESH1 game export.

Run: blender -b --factory-startup --python tools/visual/build_player_knife.py
"""

from collections import defaultdict
from pathlib import Path
import struct

import bpy


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets/visual/models/player_weapons/knife"
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)


def material(name, color, metallic, roughness, emission=0):
    result = bpy.data.materials.new(name)
    result.diffuse_color = (*color, 1)
    result.use_nodes = True
    shader = result.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1)
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Roughness"].default_value = roughness
    if emission:
        shader.inputs["Emission Color"].default_value = (*color, 1)
        shader.inputs["Emission Strength"].default_value = emission
    return result


COATED = material("0 | blade graphite", (.14, .16, .18), .75, .48)
STEEL = material("1 | worn cutting edge", (.47, .49, .50), .88, .32)
POLYMER = material("2 | grip rubber", (.045, .055, .062), .02, .82)
RECESS = material("3 | slots", (.013, .018, .021), .18, .9)
CYAN = material("4 | power indicator", (.02, .77, .90), .05, .26, 2)
PAINT = material("5 | worn orange grip armor", (.82, .32, .05), .50, .5)
MATERIAL_IDS = {COATED: 0, STEEL: 1, POLYMER: 2,
                RECESS: 3, CYAN: 4, PAINT: 5}
paint_image = bpy.data.images.load(str(ROOT /
    "assets/visual/models/worker_reference/worker_paint_albedo.png"))
paint_node = PAINT.node_tree.nodes.new("ShaderNodeTexImage")
paint_node.image = paint_image
PAINT.node_tree.links.new(paint_node.outputs["Color"],
    PAINT.node_tree.nodes.get("Principled BSDF").inputs["Base Color"])
objects = []


def from_game(value):
    return (value[0], -value[2], value[1])


def register(obj, name, mat, part=0, bevel=0):
    obj.name = name
    obj.data.materials.append(mat)
    obj["weapon_part"] = part
    if bevel:
        edge = obj.modifiers.new("machined edge", "BEVEL")
        edge.width = bevel
        edge.segments = 1
        obj.modifiers.new("weighted face normals", "WEIGHTED_NORMAL")
    objects.append(obj)
    return obj


def box(name, center, size, mat, bevel=.004, part=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=from_game(center))
    obj = bpy.context.object
    obj.dimensions = (size[0], size[2], size[1])
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return register(obj, name, mat, part, bevel)


def bolt(name, center):
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=.010,
        depth=.007, location=from_game(center))
    obj = bpy.context.object
    obj.rotation_euler.y = 1.5707963
    return register(obj, name, STEEL)


def blade(name, sections, mat):
    # A ridged diamond section forms a real cutting edge rather than a flat box.
    # Each section is (z, center_x, half_width, half_thickness).
    vertices = []
    for z, x, w, t in sections:
        for point in ((x-w, .015, z), (x, .015+t, z),
                      (x+w, .015, z), (x, .015-t, z)):
            vertices.append(from_game(point))
    faces = []
    for section in range(len(sections)-1):
        for side in range(4):
            a = 4*section+side
            b = 4*section+(side+1)%4
            faces.append((a, b, b+4, a+4))
    faces.append((3, 2, 1, 0))
    faces.append(tuple(range(len(vertices)-4, len(vertices))))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    uv = mesh.uv_layers.new(name="blade grain")
    for polygon in mesh.polygons:
        for loop_id in polygon.loop_indices:
            point = mesh.vertices[mesh.loops[loop_id].vertex_index].co
            uv.data[loop_id].uv = (point.x*3+point.y, point.y*1.8)
    return register(obj, name, mat)


# Solid butt and contoured rubber grip remain in the approved hand-contact
# envelope, with the bottom surface touching the dominant palm at y=-.038.
box("grip tang", (0, .015, -.13), (.093, .072, .42), STEEL, .007)
box("ergonomic grip", (0, .015, -.13), (.127, .098, .45), POLYMER,
    .015, part=9)
box("rear pommel", (0, .015, -.362), (.164, .115, .058), COATED, .009)
box("pommel orange rim", (0, .015, -.326), (.142, .108, .016), PAINT, .004)
box("finger guard", (0, -.042, .105), (.212, .071, .044), COATED, .006)
box("upper guard", (0, .057, .105), (.170, .054, .044), STEEL, .004)
for side in (-1, 1):
    box(f"orange grip armor {side}", (side*.069, .021, -.145),
        (.015, .071, .32), PAINT, .006)
    box(f"rubber stipple insert {side}", (side*.078, -.001, -.15),
        (.005, .046, .21), RECESS, .003)
    for z in (-.282, -.005):
        bolt(f"grip fastener {side} {z}", (side*.082, .030, z))
    box(f"blade orange root guard {side}", (side*.071, .014, .155),
        (.013, .045, .115), PAINT, .004)
    box(f"blade recessed channel {side}", (side*.105, .015, .325),
        (.004, .016, .165), RECESS, .002)
    box(f"cyan guarded indicator {side}", (side*.114, .015, .171),
        (.005, .023, .062), CYAN, .002)

blade("angular dark blade", [(.10, 0, .063, .029),
    (.19, 0, .100, .030), (.36, 0, .112, .025),
    (.49, -.012, .090, .018), (.57, -.048, .052, .010),
    (.665, -.088, .001, .001)], COATED)
for side in (-1, 1):
    # Narrow edge strip reads as honed exposed metal; the inset stays dark.
    box(f"honed edge {side}", (side*.089, .014, .34),
        (.010, .011, .27), STEEL, .001)
for tooth in range(4):
    box(f"spine notch {tooth}", (-.098, .015, .267+tooth*.055),
        (.015, .029, .016), STEEL, .001)

groups = defaultdict(lambda: {"vertices": [], "indices": [], "lookup": {}})
depsgraph = bpy.context.evaluated_depsgraph_get()
depsgraph.update()
for obj in objects:
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    mesh.calc_loop_triangles()
    normal_matrix = evaluated.matrix_world.to_3x3().inverted().transposed()
    group = groups[(int(obj["weapon_part"]), MATERIAL_IDS[obj.data.materials[0]])]
    for triangle in mesh.loop_triangles:
        for loop_id in triangle.loops:
            loop = mesh.loops[loop_id]
            position = evaluated.matrix_world @ mesh.vertices[loop.vertex_index].co
            normal = (normal_matrix @ mesh.corner_normals[loop_id].vector).normalized()
            uv = mesh.uv_layers.active.data[loop_id].uv
            vertex = (position.x, position.z, -position.y,
                      normal.x, normal.z, -normal.y, uv.x, uv.y)
            key = tuple(round(value, 5) for value in vertex)
            index = group["lookup"].get(key)
            if index is None:
                index = len(group["vertices"])
                group["lookup"][key] = index
                group["vertices"].append(vertex)
            group["indices"].append(index)
    evaluated.to_mesh_clear()

asset = OUT / "player_knife.isemesh"
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
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "player_knife.blend"))
print(f"KNIFE_EXPORT={asset} GROUPS={len(groups)} TRIANGLES="
      f"{sum(len(group['indices']) for group in groups.values()) // 3}")

"""Build the editable pistol and ISEMESH1 game export.

Run: blender -b --factory-startup --python tools/visual/build_player_pistol.py
"""

from collections import defaultdict
from pathlib import Path
import math
import struct

import bpy


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets/visual/models/player_weapons/pistol"
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


# IDs match WeaponMaterial; separate magazine and charging handle can animate.
COATED = material("0 | dark coated steel", (.14, .16, .18), .7, .5)
STEEL = material("1 | machined steel", (.42, .45, .47), .8, .35)
POLYMER = material("2 | molded graphite", (.055, .067, .075), .05, .8)
RECESS = material("3 | vents and gaps", (.016, .021, .025), .15, .85)
CYAN = material("4 | cyan status", (.02, .78, .9), .05, .25, 2)
PAINT = material("5 | worn orange armor", (.83, .33, .055), .5, .5)
MATERIAL_IDS = {COATED: 0, STEEL: 1, POLYMER: 2, RECESS: 3,
                CYAN: 4, PAINT: 5}
paint_image = bpy.data.images.load(str(ROOT /
    "assets/visual/models/worker_reference/worker_paint_albedo.png"))
paint_node = PAINT.node_tree.nodes.new("ShaderNodeTexImage")
paint_node.image = paint_image
PAINT.node_tree.links.new(paint_node.outputs["Color"],
    PAINT.node_tree.nodes.get("Principled BSDF").inputs["Base Color"])
objects = []


def from_game(v):
    return (v[0], -v[2], v[1])


def register(obj, name, mat, part, bevel=0):
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


def box(name, center, size, mat, bevel=.004, part=0, tilt=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=from_game(center))
    obj = bpy.context.object
    obj.dimensions = (size[0], size[2], size[1])
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.rotation_euler.x = math.radians(tilt)
    return register(obj, name, mat, part, bevel)


def barrel(name, center, radius, length, mat, vertices=12, part=0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=length, location=from_game(center))
    obj = bpy.context.object
    obj.rotation_euler.x = math.pi / 2
    return register(obj, name, mat, part)


def bolt(name, x, y, z, part=0, radius=.008):
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=radius,
        depth=.006, location=from_game((x, y, z)))
    obj = bpy.context.object
    obj.rotation_euler.y = math.pi / 2
    return register(obj, name, STEEL, part)


# Lower frame and compact high-bore slide mirror the side-sheet silhouette.
# The mechanism group is the complete slide assembly; it moves together.
box("angular lower frame", (0, -.062, .015), (.181, .111, .335),
    POLYMER, .010)
box("underbarrel rail housing", (0, -.125, .095), (.167, .057, .205),
    STEEL, .006)
box("trigger housing core", (0, -.105, -.090), (.159, .077, .175),
    COATED, .006)
box("slide structural carrier", (0, .065, .025), (.205, .085, .517),
    STEEL, .010, 2)
box("slide upper ridge", (0, .117, .022), (.175, .030, .494),
    COATED, .006, 2)
box("slide muzzle armor", (0, .074, .231), (.215, .076, .120),
    PAINT, .008, 2)
box("slide rear armor", (0, .077, -.195), (.211, .080, .105),
    PAINT, .006, 2)
barrel("heavy short barrel", (0, .054, .181), .038, .238, STEEL,
    part=2)
barrel("front bore", (0, .054, .302), .028, .006, RECESS, part=2)
for side in (-1, 1):
    box(f"slide long orange shield {side}", (side*.112, .082, .058),
        (.018, .084, .255), PAINT, .006, 2)
    box(f"slide dark undercut {side}", (side*.121, .021, .053),
        (.006, .027, .264), RECESS, .001, 2)
    box(f"slide machined stripe {side}", (side*.122, .034, .055),
        (.005, .007, .231), STEEL, .001, 2)
    box(f"slide recessed vent strip {side}", (side*.123, .090, -.092),
        (.005, .033, .094), RECESS, .001, 2)
    for groove in range(4):
        box(f"rear slide serration {side}-{groove}",
            (side*.123, .068, -.214+groove*.019),
            (.006, .078, .006), RECESS, .001, 2, tilt=12)
    for z in (-.207, .181):
        bolt(f"slide screw {side} {z}", side*.129, .065, z, 2)
    box(f"frame mid plate {side}", (side*.095, -.061, .070),
        (.010, .055, .165), COATED, .004)
    box(f"frame orange thumb armor {side}", (side*.095, -.084, -.167),
        (.014, .073, .085), PAINT, .005)
    box(f"underbarrel cyan status {side}", (side*.089, -.128, .147),
        (.006, .007, .055), CYAN, .001)
    bolt(f"frame pin {side}", side*.104, -.060, .020)

# Open trigger guard, steel trigger and compact cyan-accented iron sights.
box("trigger guard rear", (0, -.155, -.034), (.017, .095, .017), STEEL)
box("trigger guard front", (0, -.165, .111), (.017, .100, .017), STEEL)
box("trigger guard floor", (0, -.219, .039), (.017, .017, .162), STEEL)
box("trigger", (0, -.164, -.009), (.014, .074, .014),
    COATED, tilt=-15)
box("rear sight left ear", (-.047, .155, -.199),
    (.028, .056, .028), COATED, .003, 2)
box("rear sight right ear", (.047, .155, -.199),
    (.028, .056, .028), COATED, .003, 2)
box("rear sight cyan tip", (0, .181, -.198),
    (.015, .006, .008), CYAN, .001, 2)
box("front sight", (0, .154, .251), (.028, .046, .024),
    COATED, .003, 2)
box("front sight cyan tip", (0, .181, .251),
    (.013, .006, .007), CYAN, .001, 2)

# Pistol grip retains its original semantic region and reload contact.
box("ergonomic pistol grip", (0, -.220, -.115), (.127, .275, .150),
    POLYMER, .010, 9, tilt=22)
for side in (-1, 1):
    box(f"grip orange rear shoulder {side}", (side*.070, -.155, -.169),
        (.011, .100, .065), PAINT, .004, 9, tilt=22)
    box(f"grip patterned inset {side}", (side*.068, -.233, -.107),
        (.005, .150, .095), RECESS, .001, 9, tilt=22)
    for row in range(4):
        box(f"grip traction line {side}-{row}",
            (side*.072, -.178-row*.034, -.102),
            (.004, .005, .079), COATED, .001, 9)
    bolt(f"grip fastener {side}", side*.079, -.177, -.105, 9)
box("grip heel", (0, -.353, -.084), (.144, .036, .170),
    COATED, .005, 9)

# Slide and magazine are distinct rigid parts in the engine export.
box("straight magazine body", (0, -.245, -.115),
    (.100, .219, .104), COATED, .005, 1, tilt=22)
box("magazine base", (0, -.351, -.072), (.130, .030, .143),
    POLYMER, .004, 1, tilt=22)
for side in (-1, 1):
    box(f"magazine orange index {side}", (side*.054, -.285, -.094),
        (.004, .094, .026), PAINT, .001, 1, tilt=22)
    box(f"slide side release {side}", (side*.127, .008, -.076),
        (.017, .020, .050), COATED, .003, 2)
# Export evaluated bevels and UVs without joining editable Blender parts.
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
                group["vertices"].append(key)
            group["indices"].append(index)
    evaluated.to_mesh_clear()

asset = OUT / "player_pistol.isemesh"
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
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "player_pistol.blend"))
print(f"PISTOL_EXPORT={asset} GROUPS={len(groups)} TRIANGLES="
      f"{sum(len(group['indices']) for group in groups.values()) // 3}")

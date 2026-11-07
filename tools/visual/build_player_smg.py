"""Build the editable submachine gun and ISEMESH1 game export.

Run: blender -b --factory-startup --python tools/visual/build_player_smg.py
"""

from collections import defaultdict
from pathlib import Path
import math
import struct

import bpy


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets/visual/models/player_weapons/smg"
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


# Short dense receiver and shallow handguard retain the older contact envelope.
box("receiver cast chassis", (0, .010, -.015), (.202, .155, .48), POLYMER, .014)
box("receiver top spine", (0, .100, .010), (.179, .033, .53), STEEL, .007)
box("receiver lower lock", (0, -.090, .020), (.17, .027, .43), COATED, .005)
box("fore-end core", (0, .004, .400), (.174, .146, .40), POLYMER, .011)
box("fore-end lower keel", (0, -.075, .41), (.12, .018, .34), STEEL, .003)
box("full upper rail", (0, .111, .22), (.151, .023, .79), COATED, .004)
for side in (-1, 1):
    box(f"receiver armor upper {side}", (side*.108, .054, -.055),
        (.019, .075, .29), PAINT, .006)
    box(f"receiver diagonal orange index {side}", (side*.111, -.042, .044),
        (.015, .035, .12), PAINT, .003)
    box(f"handguard orange shell {side}", (side*.095, .029, .425),
        (.019, .116, .305), PAINT, .007)
    box(f"handguard dark vent channel {side}", (side*.106, -.027, .425),
        (.005, .034, .210), RECESS, .001)
    for slot in range(3):
        box(f"handguard vent divider {side}-{slot}",
            (side*.109, -.027, .35+slot*.074), (.006, .040, .012),
            STEEL, .001)
    for z, y in ((-.17, .07), (.14, .04), (.33, .065), (.54, .065)):
        bolt(f"panel screw {side} {z}", side*.121, y, z)
    box(f"folding stock top rail {side}", (side*.083, .009, -.423),
        (.022, .024, .30), STEEL, .003)
    box(f"folding stock lower brace {side}", (side*.083, -.061, -.433),
        (.021, .024, .24), COATED, .003, tilt=17)
    box(f"stock orange cheek {side}", (side*.106, .012, -.552),
        (.010, .065, .105), PAINT, .003)

# Distinct folding stock, short vented barrel and plain combat sights.
box("stock folding hinge", (0, -.012, -.310), (.177, .106, .071), STEEL, .006)
box("stock butt", (0, -.030, -.587), (.194, .159, .057), POLYMER, .006)
box("stock rubber end", (0, -.030, -.619), (.188, .155, .013), RECESS, .002)
barrel("short barrel", (0, .006, .602), .032, .375, STEEL)
barrel("compact muzzle sleeve", (0, .006, .772), .048, .082, COATED)
barrel("dark muzzle opening", (0, .006, .815), .029, .005, RECESS)
box("front cyan status slot", (0, -.066, .61), (.037, .006, .061), CYAN, .001)
for notch in range(7):
    box(f"rail notch {notch}", (0, .127, -.20+notch*.11),
        (.145, .006, .012), RECESS, .001)
box("rear iron sight", (0, .150, -.200), (.075, .045, .032), COATED, .003)
box("rear sight aperture", (0, .159, -.199), (.029, .031, .006), RECESS, .001)
box("front iron sight", (0, .149, .566), (.058, .050, .030), COATED, .003)
box("front sight bead", (0, .175, .566), (.011, .011, .009), CYAN, .001)

# Grip remains centered on the runtime hand-contact position.
box("textured pistol grip", (0, -.184, -.110), (.130, .275, .140),
    POLYMER, .010, 9, tilt=18)
for side in (-1, 1):
    box(f"grip side inset {side}", (side*.068, -.195, -.110),
        (.005, .165, .086), RECESS, .001, 9, tilt=18)
box("trigger guard rear", (0, -.127, .020), (.018, .090, .017), STEEL)
box("trigger guard front", (0, -.174, .108), (.018, .091, .017), STEEL)
box("trigger guard floor", (0, -.218, .063), (.018, .015, .108), STEEL)
box("trigger", (0, -.171, .022), (.014, .061, .014), COATED,
    tilt=-15)

# Magazine and charging handle are rigid runtime animation groups.
box("long slim magazine", (0, -.219, .20), (.127, .360, .108),
    COATED, .007, 1, tilt=-8)
box("magazine reinforced floor", (0, -.385, .244), (.145, .038, .118),
    POLYMER, .004, 1, tilt=-8)
for side in (-1, 1):
    box(f"magazine orange length stripe {side}", (side*.068, -.215, .241),
        (.006, .255, .026), PAINT, .001, 1, tilt=-8)
    for rib in range(3):
        box(f"magazine cross rib {side}-{rib}",
            (side*.070, -.136-rib*.063, .207),
            (.006, .006, .096), STEEL, .001, 1)
box("charging handle stem", (-.118, -.018, -.170),
    (.042, .023, .037), STEEL, .003, 2)
box("charging handle pull", (-.151, -.019, -.170),
    (.018, .041, .036), COATED, .003, 2)

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

asset = OUT / "player_smg.isemesh"
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
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "player_smg.blend"))
print(f"SMG_EXPORT={asset} GROUPS={len(groups)} TRIANGLES="
      f"{sum(len(group['indices']) for group in groups.values()) // 3}")

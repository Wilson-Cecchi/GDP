"""Build the editable assault rifle and ISEMESH1 game export.

Run: blender -b --factory-startup --python tools/visual/build_player_rifle.py
"""

from collections import defaultdict
from pathlib import Path
import math
import struct

import bpy


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets/visual/models/player_weapons/rifle"
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


# Material IDs deliberately match WeaponMaterial in WeaponViewmodelAssets.hpp.
COATED = material("0 | dark coated steel", (0.14, .16, .18), .7, .5)
STEEL = material("1 | machined steel", (.42, .45, .47), .8, .35)
POLYMER = material("2 | molded graphite", (.055, .067, .075), .05, .8)
RECESS = material("3 | vents and gaps", (.016, .021, .025), .15, .85)
CYAN = material("4 | cyan optics", (.02, .78, .9), .05, .25, 2)
PAINT = material("5 | worn orange armor", (.83, .33, .055), .5, .5)
MATERIAL_IDS = {COATED: 0, STEEL: 1, POLYMER: 2, RECESS: 3, CYAN: 4, PAINT: 5}
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


def bolt(name, x, y, z, radius=.009):
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=radius,
        depth=.006, location=from_game((x, y, z)))
    obj = bpy.context.object
    obj.rotation_euler.y = math.pi / 2
    return register(obj, name, STEEL, 0)


# Receiver and separated painted armor. Paint is a shared worn material with
# the enemy family; actual chips are sampled from worker_paint_albedo.png.
box("receiver dark chassis", (0, .025, 0), (.215, .175, .51), POLYMER, .021)
box("receiver upper skeleton", (0, .108, .01), (.19, .05, .54), STEEL, .009)
box("receiver underside", (0, -.073, .015), (.19, .038, .47), COATED, .008)
box("handguard structural core", (0, .015, .52), (.17, .146, .61), POLYMER, .016)
box("handguard upper rail", (0, .108, .51), (.17, .025, .67), STEEL, .005)
box("handguard lower keel", (0, -.063, .53), (.12, .017, .48), STEEL, .005)
for side in (-1, 1):
    box(f"receiver orange upper armor {side}", (side*.116, .066, -.01),
        (.019, .085, .30), PAINT, .008)
    box(f"receiver orange lower armor {side}", (side*.117, -.04, .015),
        (.016, .045, .23), PAINT, .005)
    box(f"handguard orange armor {side}", (side*.097, .048, .54),
        (.021, .098, .37), PAINT, .009)
    box(f"handguard reinforced nose {side}", (side*.09, -.026, .73),
        (.027, .128, .095), PAINT, .007)
    box(f"stock orange cheek plate {side}", (side*.066, .077, -.47),
        (.015, .045, .28), PAINT, .006)
    box(f"rear stock open rail {side}", (side*.083, -.076, -.48),
        (.025, .028, .34), STEEL, .005)
    box(f"handguard vent recess {side}", (side*.109, -.02, .53),
        (.009, .027, .24), RECESS, .002)
    for i in range(3):
        box(f"vent rib {side}-{i}", (side*.115, -.02, .43+i*.09),
            (.009, .033, .012), STEEL, .001)
    for z in (-.11, .11, .37, .69):
        bolt(f"armor screw {side} {z}", side*.13, .079, z)

# True open stock, dark receiver grip and separately animated magazine.
barrel("stock buffer tube", (0, -.015, -.45), .039, .43, STEEL)
box("stock adjustable butt", (0, -.025, -.67), (.20, .17, .075), POLYMER, .010)
box("stock rubber pad", (0, -.025, -.718), (.195, .17, .024), RECESS, .005)
box("ergonomic hand grip", (0, -.18, -.095), (.13, .26, .16), POLYMER, .012,
    part=9, tilt=16)
for side in (-1, 1):
    for i in range(4):
        box(f"grip stippling {side}-{i}", (side*.068, -.125-i*.035, -.092),
            (.004, .007, .075), RECESS, .001)
box("trigger guard top", (0, -.10, .022), (.017, .015, .19), STEEL)
box("trigger guard front", (0, -.18, .105), (.017, .14, .018), STEEL)
box("trigger guard bottom", (0, -.245, .035), (.017, .015, .15), STEEL)
box("straight magazine", (0, -.20, .215), (.13, .35, .11), COATED, .007, 1,
    tilt=-8)
box("magazine floor", (0, -.37, .26), (.15, .037, .13), POLYMER, .005, 1)
for side in (-1, 1):
    box(f"magazine orange index {side}", (side*.07, -.24, .247),
        (.004, .17, .025), PAINT, .001, 1)
    for i in range(3):
        box(f"magazine rib {side}-{i}", (side*.072, -.11-i*.056, .235),
            (.005, .005, .08), STEEL, .001, 1)

# Barrel, muzzle brake and rail notches remain separate from the armor.
barrel("mid barrel", (0, 0, .91), .040, .55, STEEL)
barrel("muzzle brake sleeve", (0, 0, 1.047), .057, .096, COATED)
barrel("muzzle exit rim", (0, 0, 1.095), .047, .016, STEEL)
for i in range(6):
    box(f"top rail notch {i}", (0, .137, .13+i*.12),
        (.145, .011, .016), RECESS, .001)

# Holographic optic is an open frame, with a subtle emitter in the base.
box("optic mount", (0, .158, -.07), (.155, .035, .15), RECESS, .006)
box("optic left wall", (-.074, .221, -.07), (.023, .11, .075), COATED, .004)
box("optic right wall", (.074, .221, -.07), (.023, .11, .075), COATED, .004)
box("optic roof", (0, .282, -.07), (.165, .021, .075), STEEL, .003)
box("optic cyan reticle emitter", (0, .166, -.111), (.042, .006, .009), CYAN,
    .001)
box("charging handle", (-.133, -.017, -.17), (.028, .020, .09), STEEL,
    .003, 2)

# Export all Blender objects in rigid-part/material batches.
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

asset = OUT / "player_rifle.isemesh"
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
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "player_rifle.blend"))
print(f"RIFLE_EXPORT={asset} GROUPS={len(groups)} TRIANGLES="
      f"{sum(len(group['indices']) for group in groups.values()) // 3}")

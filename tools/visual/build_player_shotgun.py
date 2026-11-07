"""Build the editable pump shotgun and its ISEMESH1 game export.

Run: blender -b --factory-startup --python tools/visual/build_player_shotgun.py

All positions below are in game weapon coordinates: X across the gun, Y up,
Z toward the muzzle. The mesh keeps the pump in rigid part 2 and the grip in
semantic part 9. The temporary reload shell belongs to the C++ implementation.
"""

from collections import defaultdict
from pathlib import Path
import math
import struct

import bpy


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets/visual/models/player_weapons/shotgun"
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


# Keep IDs in sync with WeaponMaterial; the game assigns material maps by ID.
COATED = material("0 | dark coated steel", (.14, .16, .18), .7, .5)
STEEL = material("1 | machined steel", (.42, .45, .47), .8, .35)
POLYMER = material("2 | molded graphite", (.055, .067, .075), .05, .8)
RECESS = material("3 | vents and gaps", (.016, .021, .025), .15, .85)
CYAN = material("4 | cyan status emitters", (.02, .78, .9), .05, .25, 2)
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


def tube(name, center, radius, length, mat, vertices=12, part=0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=length, location=from_game(center))
    obj = bpy.context.object
    obj.rotation_euler.x = math.pi / 2
    return register(obj, name, mat, part)


def side_bolt(name, side, y, z, radius=.009, part=0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=radius,
        depth=.006, location=from_game((side * .125, y, z)))
    obj = bpy.context.object
    obj.rotation_euler.y = math.pi / 2
    return register(obj, name, STEEL, part)


# Receiver: dark machined core, inset access panels, raised worn armor.
box("wide receiver chassis", (0, .018, .005), (.230, .195, .510), POLYMER, .017)
box("receiver crown", (0, .116, -.01), (.212, .040, .488), COATED, .008)
box("receiver steel lower frame", (0, -.092, .01), (.217, .036, .46), STEEL, .006)
box("receiver ejection recess", (.123, .036, .030), (.006, .082, .155), RECESS, .001)
box("receiver ejection rim", (.128, .036, .027), (.007, .009, .168), STEEL, .001)
box("trigger guard upper", (0, -.111, -.073), (.021, .016, .151), STEEL, .002)
box("trigger guard bow", (0, -.187, -.005), (.021, .143, .018), STEEL, .002)
box("trigger guard lower", (0, -.258, -.074), (.021, .016, .151), STEEL, .002)
box("trigger", (0, -.174, -.088), (.012, .056, .016), RECESS, .001, tilt=-15)
for side in (-1, 1):
    box(f"receiver top armor {side}", (side * .117, .086, -.075),
        (.020, .085, .305), PAINT, .007)
    box(f"receiver cheek plate {side}", (side * .122, -.022, .036),
        (.016, .088, .270), COATED, .005)
    box(f"receiver inset service hatch {side}", (side * .132, -.016, -.031),
        (.006, .065, .130), STEEL, .002)
    box(f"receiver armor toe {side}", (side * .124, -.061, .159),
        (.014, .050, .078), PAINT, .004)
    for z in (-.19, -.035, .158):
        side_bolt(f"receiver captive screw {side} {z}", side, .044, z)

# Solid adjustable stock with an open structural recess like the reference.
tube("stock buffer cylinder", (0, .008, -.375), .055, .265, STEEL)
box("stock root", (0, -.004, -.492), (.186, .161, .250), COATED, .012)
box("stock rear butt", (0, -.018, -.694), (.226, .229, .084), POLYMER, .011)
box("stock recoil pad", (0, -.018, -.745), (.223, .224, .022), RECESS, .004)
box("stock lower structural bridge", (0, -.100, -.594), (.150, .039, .190), STEEL, .005)
for side in (-1, 1):
    box(f"stock worn cheek armor {side}", (side * .103, .048, -.570),
        (.018, .106, .199), PAINT, .007)
    box(f"stock inset opening {side}", (side * .115, -.035, -.585),
        (.005, .055, .093), RECESS, .002)
    side_bolt(f"stock rear fastener {side}", side, .037, -.681)

# Grip is a separate semantic region for the player's support/contact logic.
box("ergonomic pistol grip", (0, -.180, -.213), (.132, .257, .145), POLYMER,
    .012, part=9, tilt=24)
for side in (-1, 1):
    box(f"grip inset rubber {side}", (side * .068, -.185, -.215),
        (.005, .167, .073), RECESS, .002, part=9, tilt=24)
    box(f"grip orange heel {side}", (side * .066, -.290, -.213),
        (.008, .024, .112), PAINT, .002, part=9)

# Paired barrel and magazine tube are independent silhouettes. End rings have
# visible dark bore; their axes match the animated pump's stroke direction.
tube("thick upper barrel", (0, .049, .662), .052, .972, STEEL, 16)
tube("upper barrel armored jacket", (0, .049, .469), .060, .278, COATED, 16)
tube("upper muzzle collar", (0, .049, 1.139), .069, .053, COATED, 16)
tube("upper muzzle steel lip", (0, .049, 1.171), .063, .012, STEEL, 16)
tube("dark bore", (0, .049, 1.180), .042, .004, RECESS, 16)
tube("lower magazine tube", (0, -.076, .632), .046, .881, COATED, 12)
tube("lower tube front cap", (0, -.076, 1.081), .053, .035, STEEL, 12)
for z in (.265, .833, 1.076):
    tube(f"barrel support collar {z}", (0, -.017, z), .071, .036, COATED, 12)

# Heavy fixed handguard, high orange armor, vent slots and small HUD emitter.
box("fixed handguard spine", (0, .125, .637), (.181, .035, .884), COATED, .005)
for side in (-1, 1):
    box(f"long orange handguard shell {side}", (side * .108, .107, .658),
        (.020, .074, .726), PAINT, .009)
    box(f"front reinforced orange shield {side}", (side * .109, .037, 1.058),
        (.020, .142, .112), PAINT, .009)
    box(f"black lower handguard rail {side}", (side * .103, -.083, .821),
        (.027, .030, .366), COATED, .004)
    for z in (.398, .735, 1.043):
        side_bolt(f"handguard clamp fastener {side} {z}", side, .108, z)
    for z in (.772, .848, .924):
        box(f"handguard cooling slot {side} {z}", (side * .119, .119, z),
            (.005, .016, .042), RECESS, 0)
    box(f"cyan receiver diagnostic {side}", (side * .135, .029, .184),
        (.006, .013, .076), CYAN, .001)
    box(f"cyan front safety marker {side}", (side * .122, -.063, 1.060),
        (.006, .054, .010), CYAN, .001)

# Pump remains one movable rigid part. Its central grip is at z=.49, with a
# lower left support-hand contact near (-.030,-.120,.410).
tube("pump internal bearing", (0, -.076, .495), .070, .322, RECESS, 12, 2)
box("pump graphite sleeve", (0, -.109, .494), (.204, .107, .316), POLYMER,
    .012, 2)
box("pump lower palm surface", (0, -.157, .481), (.135, .028, .252), COATED,
    .006, 2)
for side in (-1, 1):
    box(f"pump orange front armor {side}", (side * .108, -.096, .614),
        (.019, .114, .067), PAINT, .006, 2)
    box(f"pump orange rear armor {side}", (side * .108, -.096, .379),
        (.019, .114, .067), PAINT, .006, 2)
    for i in range(5):
        z = .414 + i * .039
        box(f"pump longitudinal grip rib {side} {i}",
            (side * .112, -.116, z), (.007, .070, .011), RECESS, 0, 2)
    side_bolt(f"pump front hinge {side}", side, -.092, .617, .007, 2)

# Rugged top rail and open iron sights. No optic blocks the close-range view.
for i in range(8):
    box(f"top rail tooth {i}", (0, .150, -.105 + i * .140),
        (.144, .016, .025), RECESS, 0)
box("rear sight base", (0, .176, -.162), (.112, .032, .072), COATED, .004)
box("rear sight left ear", (-.054, .219, -.162), (.019, .080, .064), STEEL, .003)
box("rear sight right ear", (.054, .219, -.162), (.019, .080, .064), STEEL, .003)
box("front sight base", (0, .177, 1.009), (.113, .030, .067), COATED, .003)
box("front sight post", (0, .219, 1.009), (.019, .066, .015), STEEL, .002)


# Export evaluated Blender geometry by rigid part and material. The engine
# keeps part 1 free for a procedural shell visible during per-shell reload.
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

asset = OUT / "player_shotgun.isemesh"
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
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "player_shotgun.blend"))
print(f"SHOTGUN_EXPORT={asset} GROUPS={len(groups)} TRIANGLES="
      f"{sum(len(group['indices']) for group in groups.values()) // 3}")

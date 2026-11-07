"""Build the editable Sniper and its ISEMESH1 game export.

Run: blender -b --factory-startup --python tools/visual/build_player_sniper.py

Game coordinates: X right, Y up, Z forward. The part and material IDs are the
same as the existing WeaponViewmodelAssets contract; this script only creates
source/export assets and leaves game integration to the coordinator.
"""

from collections import defaultdict
from pathlib import Path
import math
import struct

import bpy


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets/visual/models/player_weapons/sniper"
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


COATED = material("0 | dark coated steel", (.14, .16, .18), .7, .5)
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


def box(name, center, size, mat, bevel=.003, part=0, tilt=0):
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


def side_disc(name, x, y, z, radius=.009, mat=STEEL, vertices=8):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
        depth=.006, location=from_game((x, y, z)))
    obj = bpy.context.object
    obj.rotation_euler.y = math.pi / 2
    return register(obj, name, mat)


# Graphite receiver and long, open front structure. Orange covers have genuine
# edge depth; the gaps between panels expose dark mechanical parts.
box("receiver graphite core", (0, .013, .0), (.192, .156, .48), POLYMER, .014)
box("receiver upper machined spine", (0, .103, .00), (.174, .042, .50), STEEL, .005)
box("receiver lower reinforcement", (0, -.076, .03), (.170, .032, .43), COATED, .006)
box("handguard floating core", (0, .019, .49), (.130, .112, .60), POLYMER, .008)
box("handguard top rail", (0, .101, .52), (.143, .023, .67), STEEL, .003)
box("handguard lower rail", (0, -.049, .49), (.103, .016, .49), COATED, .003)
for side in (-1, 1):
    box(f"receiver upper painted armor {side}", (side*.103, .058, -.025),
        (.018, .078, .34), PAINT, .006)
    box(f"receiver rear painted plate {side}", (side*.10, -.035, -.18),
        (.014, .044, .12), PAINT, .004)
    box(f"handguard center orange plate {side}", (side*.078, .030, .42),
        (.016, .081, .34), PAINT, .005)
    box(f"handguard nose orange plate {side}", (side*.077, .030, .71),
        (.016, .082, .18), PAINT, .005)
    box(f"handguard black vent gutter {side}", (side*.089, -.024, .53),
        (.007, .027, .38), RECESS, .001)
    for i in range(3):
        box(f"vent bridge {side}-{i}", (side*.094, -.022, .38+i*.14),
            (.007, .031, .011), STEEL, .001)
    for z in (-.15, .04, .34, .69):
        side_disc(f"armor captive fastener {side} {z}", side*.116, .068, z)
    box(f"stock painted cheek plate {side}", (side*.064, .087, -.49),
        (.012, .043, .28), PAINT, .005)
    box(f"stock exposed lower strut {side}", (side*.075, -.063, -.50),
        (.017, .021, .33), STEEL, .002)

# Open adjustable buttstock, inclined grip, trigger frame and removable mag.
tube("stock central buffer", (0, -.012, -.50), .030, .38, STEEL)
box("buttstock vertical housing", (0, -.025, -.672),
    (.189, .17, .059), POLYMER, .008)
box("buttstock rubber pad", (0, -.025, -.711), (.187, .17, .021), RECESS, .004)
box("rear cheek rest", (0, .117, -.49), (.144, .022, .25), COATED, .004)
box("grip angled rearward", (0, -.178, -.13), (.12, .26, .135), POLYMER,
    .011, part=9, tilt=18)
for side in (-1, 1):
    for i in range(3):
        box(f"grip inset {side}-{i}", (side*.062, -.138-i*.040, -.13),
            (.004, .006, .071), RECESS, .001, part=9)
box("trigger guard front", (0, -.17, .060), (.014, .13, .015), STEEL, .001)
box("trigger guard bottom", (0, -.235, -.01), (.014, .014, .16), STEEL, .001)
box("trigger", (0, -.17, -.025), (.012, .047, .015), STEEL, .001)
box("magazine graphite box", (0, -.205, .095),
    (.111, .195, .147), COATED, .006, part=1)
box("magazine orange base", (0, -.31, .096),
    (.127, .024, .157), PAINT, .003, part=1)
for side in (-1, 1):
    box(f"magazine cyan ammunition gauge {side}",
        (side*.057, -.208, .120), (.003, .115, .009), CYAN, .001, part=1)

# Long thick barrel with exposed interval, mounting collar, angular brake.
tube("free floating inner barrel", (0, .011, .89), .036, .68, STEEL)
tube("barrel reinforcement", (0, .011, 1.03), .043, .26, COATED)
tube("muzzle brake main block", (0, .011, 1.191), .055, .10, COATED, 8)
tube("muzzle front bevel ring", (0, .011, 1.242), .051, .014, STEEL, 8)
box("muzzle side vent left", (-.054, .011, 1.193),
    (.003, .035, .047), RECESS, .001)
box("muzzle side vent right", (.054, .011, 1.193),
    (.003, .035, .047), RECESS, .001)
for z in (.83, 1.02):
    tube(f"barrel orange reinforcement collar {z}",
        (0, .011, z), .055, .038, PAINT, 12)
for i in range(5):
    box(f"handguard rail tooth {i}", (0, .119, .20+i*.13),
        (.122, .010, .015), RECESS, .001)

# Keep the familiar tall optic profile: two ring mounts, ocular and objective
# modules, adjustment turrets, cyan front glass, and orange focus housings.
optic_y = .265
box("optic mounting rail", (0, .147, .010), (.116, .024, .50), RECESS, .003)
for z in (-.09, .12):
    box(f"optic riser {z}", (0, .181, z),
        (.099, .086, .042), COATED, .004)
    tube(f"optic clamping ring {z}", (0, optic_y, z), .077, .045, STEEL)
    side_disc(f"optic mount screw {z}", .08, optic_y, z, .012)
tube("optic main tube", (0, optic_y, -.005), .056, .46, COATED)
tube("optic rear eyepiece housing", (0, optic_y, -.267), .073, .17, COATED)
tube("optic rear eye rim", (0, optic_y, -.354), .081, .016, STEEL)
tube("optic rear glass", (0, optic_y, -.365), .059, .004, RECESS)
tube("optic front tapered bell", (0, optic_y, .268), .086, .17, COATED)
tube("optic large objective rim", (0, optic_y, .355), .101, .053, STEEL)
tube("optic front inset", (0, optic_y, .385), .088, .012, RECESS)
tube("optic blue objective glass", (0, optic_y, .392), .074, .004, CYAN)
for z in (-.25, .29):
    tube(f"optic orange clamp {z}", (0, optic_y, z), .082, .040, PAINT)
box("optic upper elevation turret", (0, .346, .004),
    (.064, .073, .065), COATED, .005)
box("optic turret crown", (0, .386, .004),
    (.078, .014, .076), STEEL, .003)
side_disc("optic lateral windage turret", .067, optic_y, .008, .035, STEEL)
box("optic cyan readout", (0, .144, -.145),
    (.050, .003, .008), CYAN, .001)

# Bolt body/handle is a rigid mechanism group, anchored at the old contact.
box("bolt machined shank", (.148, -.012, -.08),
    (.136, .027, .025), STEEL, .003, part=2, tilt=25)
tube("bolt pull knob", (.215, -.070, -.080), .026, .044, STEEL,
     vertices=10, part=2)


# Export evaluated Blender meshes in rigid-part/material groups.
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
            # Bevel UV interpolation can vary by one float ulp between Blender
            # processes. Quantize the serialized values as well as the key.
            vertex = tuple(round(value, 5) for value in
                (position.x, position.z, -position.y,
                 normal.x, normal.z, -normal.y, uv.x, uv.y))
            key = vertex
            index = group["lookup"].get(key)
            if index is None:
                index = len(group["vertices"])
                group["lookup"][key] = index
                group["vertices"].append(vertex)
            group["indices"].append(index)
    evaluated.to_mesh_clear()

asset = OUT / "player_sniper.isemesh"
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
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "player_sniper.blend"))
print(f"SNIPER_EXPORT={asset} GROUPS={len(groups)} TRIANGLES="
      f"{sum(len(group['indices']) for group in groups.values()) // 3}")

"""Add an editable tactical exosuit to the existing Player_Rig Blender source.

Run from the repository root:
  blender -b -t 2 --python-exit-code 1 assets/visual/models/player_rig.blend \
    --python tools/blender/create_player_exosuit.py

The source rig is loaded but never saved. All generated files go to the
player_exosuit and player_exosuit_blender directories.
"""

import math
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets/visual/models/player_rig.blend"
OUT = ROOT / "assets/visual/models/player_exosuit"
REVIEW = ROOT / "visual/validation/player_exosuit_blender"
if Path(bpy.data.filepath).resolve() != SOURCE.resolve():
    raise RuntimeError("Open assets/visual/models/player_rig.blend before running")
if OUT.exists() and (OUT / "player_exosuit.blend").exists():
    raise RuntimeError("Output already exists; move it aside before rebuilding")
OUT.mkdir(parents=True, exist_ok=True)
REVIEW.mkdir(parents=True, exist_ok=True)

rig = bpy.data.objects["Player_Rig"]
original_bones = tuple(b.name for b in rig.data.bones)
original_actions = tuple(sorted(a.name for a in bpy.data.actions))
original_sockets = tuple(sorted(o.name for o in bpy.data.objects
                                if o.name.startswith("SOCKET_")))
if len(original_bones) != 19 or len(original_actions) != 6:
    raise RuntimeError("Unexpected source rig contract")

cloth = bpy.data.materials["M01_Graphite_Cloth"]
trim = bpy.data.materials["M02_Stitched_Trim"]
reinforce = bpy.data.materials["M03_Reinforcement"]
leather = bpy.data.materials["M04_Belt_Leather"]
metal = bpy.data.materials["M05_Blackened_Metal"]
rubber = bpy.data.materials["M06_Rubber"]
# Keep the six stable names, but tune only this review copy toward the warm,
# matte technical fabric and contrasting metal of the four-view reference.
for mat, color, roughness, metallic in (
    (cloth, (.018, .021, .024), .94, 0),
    (trim, (.078, .069, .059), .79, .12),
    (reinforce, (.025, .028, .031), .66, .33),
    (leather, (.065, .048, .035), .87, .05),
    (metal, (.15, .145, .138), .48, .67),
    (rubber, (.011, .013, .015), .92, 0),
):
    shader = mat.node_tree.nodes.get("Principled BSDF")
    mat.diffuse_color = (*color, 1)
    shader.inputs["Base Color"].default_value = (*color, 1)
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Metallic"].default_value = metallic

cloth_shader = cloth.node_tree.nodes.get("Principled BSDF")
cloth_noise = cloth.node_tree.nodes.new("ShaderNodeTexNoise")
cloth_noise.inputs["Scale"].default_value = 115
cloth_noise.inputs["Detail"].default_value = 2
cloth_ramp = cloth.node_tree.nodes.new("ShaderNodeValToRGB")
cloth_ramp.color_ramp.elements[0].position = .20
cloth_ramp.color_ramp.elements[0].color = (.018, .020, .022, 1)
cloth_ramp.color_ramp.elements[1].position = .80
cloth_ramp.color_ramp.elements[1].color = (.047, .049, .048, 1)
cloth.node_tree.links.new(cloth_noise.outputs["Fac"], cloth_ramp.inputs["Fac"])
cloth.node_tree.links.new(cloth_ramp.outputs["Color"], cloth_shader.inputs["Base Color"])


def material(name, color, metallic, roughness, emission=0):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1)
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Roughness"].default_value = roughness
    if emission:
        shader.inputs["Emission Color"].default_value = (*color, 1)
        shader.inputs["Emission Strength"].default_value = emission
    return mat


paint = material("M07_Worn_Orange", (.83, .34, .045), .48, .53)
paint_image = bpy.data.images.load(str(ROOT /
    "assets/visual/models/worker_reference/worker_paint_albedo.png"))
paint_node = paint.node_tree.nodes.new("ShaderNodeTexImage")
paint_node.image = paint_image
paint.node_tree.links.new(paint_node.outputs["Color"],
                          paint.node_tree.nodes.get("Principled BSDF").inputs["Base Color"])
cyan = material("M08_Cyan_Visor", (.015, .56, .88), .22, .22, 1.4)

collection = bpy.data.collections.new("Player_Exosuit_Armor")
bpy.context.scene.collection.children.link(collection)
new_objects = []


def register(obj, name, mat, weights):
    obj.name = name
    for old_collection in list(obj.users_collection):
        old_collection.objects.unlink(obj)
    collection.objects.link(obj)
    obj.data.materials.append(mat)
    obj.parent = rig
    for bone_name, weight in weights.items():
        group = obj.vertex_groups.new(name=bone_name)
        group.add(list(range(len(obj.data.vertices))), weight, "REPLACE")
    modifier = obj.modifiers.new("Armor_Follows_Rig", "ARMATURE")
    modifier.object = rig
    modifier.use_deform_preserve_volume = True
    new_objects.append(obj)
    return obj


def one(bone):
    return {bone: 1.0}


def box(name, center, half, mat, bone, bevel=.008, angle=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.object
    obj.scale = tuple(2*v for v in half)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if angle:
        obj.rotation_euler.y = math.radians(angle)
    if bevel:
        mod = obj.modifiers.new("Small_Machined_Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 1
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return register(obj, name, mat, one(bone) if isinstance(bone, str) else bone)


def ellipse(name, center, radius, mat, bone, segments=16, rings=8):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings,
                                         location=center)
    obj = bpy.context.object
    obj.scale = radius
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return register(obj, name, mat, one(bone) if isinstance(bone, str) else bone)


def disc(name, center, radius, depth, mat, bone, axis="Y", vertices=12):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=depth, location=center)
    obj = bpy.context.object
    if axis == "Y":
        obj.rotation_euler.x = math.pi/2
    elif axis == "X":
        obj.rotation_euler.y = math.pi/2
    return register(obj, name, mat, one(bone) if isinstance(bone, str) else bone)


def plate(name, polygon, y, depth, mat, bone):
    """An extruded front/back XZ polygon with real edge thickness."""
    n = len(polygon)
    verts = [(x, y, z) for x, z in polygon]
    verts += [(x, y+depth, z) for x, z in polygon]
    faces = [tuple(reversed(range(n))), tuple(range(n, 2*n))]
    faces += [(i, (i+1)%n, (i+1)%n+n, i+n) for i in range(n)]
    mesh = bpy.data.meshes.new(name+"_Mesh")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    min_x = min(x for x, _ in polygon)
    max_x = max(x for x, _ in polygon)
    min_z = min(z for _, z in polygon)
    max_z = max(z for _, z in polygon)
    uv_layer = mesh.uv_layers.new(name="Paint_UV")
    for face in mesh.polygons:
        for loop_id in face.loop_indices:
            vertex = mesh.vertices[mesh.loops[loop_id].vertex_index].co
            # Deliberately sample a worn, orange-dominant portion of the
            # shared paint rather than a random dark patch in Generated UV.
            u = .04 + .42*(vertex.x-min_x)/max(max_x-min_x, .001)
            v = .04 + .42*(vertex.z-min_z)/max(max_z-min_z, .001)
            uv_layer.data[loop_id].uv = (u, v)
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    return register(obj, name, mat, one(bone) if isinstance(bone, str) else bone)


def bolt(name, center, bone, axis="Y", radius=.011):
    disc(name+"_Socket", center, radius*1.55, .005, reinforce, bone, axis, 10)
    c = list(center)
    c[1 if axis == "Y" else 0] += -.004 if axis == "Y" else .004
    disc(name+"_Head", tuple(c), radius, .007, metal, bone, axis, 10)


# The source rig deliberately has no head. The exosuit copy makes neck a
# deform bone so the helmet and its visor follow the existing neck animations.
rig.data.bones["neck"].use_deform = True
ellipse("Helmet_Graphite_Shell", (0, -.003, 1.794), (.133, .135, .151),
        reinforce, "neck", 24, 12)
ellipse("Helmet_Upper_Crown", (0, -.025, 1.868), (.128, .111, .088),
        metal, "neck", 24, 12)
plate("Helmet_Orange_Center_Ridge", [(-.032, 1.923), (.032, 1.923),
      (.029, 1.962), (-.029, 1.962)], -.112, .009, paint, "neck")
for index, (y, z) in enumerate(((-.100, 1.955), (-.041, 1.970),
                               (.025, 1.967), (.086, 1.943))):
    box(f"Helmet_Orange_Crown_Segment.{index}", (0, y, z),
        (.028, .030, .009), paint, "neck", .006)
ellipse("Helmet_Visor_Seal", (0, -.128, 1.823), (.121, .022, .049),
        rubber, "neck", 24, 12)
for side, sign in (("L", 1), ("R", -1)):
    ellipse(f"Helmet_Temple_Armor.{side}", (sign*.105, -.043, 1.827),
        (.037, .087, .064), metal, "neck", 16, 8)
    disc(f"Helmet_Ear_Drive.{side}", (sign*.140, -.005, 1.801),
         .050, .020, reinforce, "neck", "X", 16)
    disc(f"Helmet_Ear_Orange_Ring.{side}", (sign*.154, -.005, 1.801),
         .037, .007, paint, "neck", "X", 16)
    disc(f"Helmet_Ear_Steel_Hub.{side}", (sign*.158, -.005, 1.801),
         .022, .009, metal, "neck", "X", 12)
    box(f"Helmet_Cheek.{side}", (sign*.076, -.134, 1.752),
        (.043, .024, .042), metal, "neck", .014)
    plate(f"Visor_Eye.{side}", [(sign*.012, 1.848), (sign*.106, 1.856),
          (sign*.102, 1.830), (sign*.024, 1.827)], -.161, .006, cyan, "neck")
    ellipse(f"Helmet_Brow_Segment.{side}", (sign*.066, -.135, 1.873),
        (.058, .019, .016), reinforce, "neck", 16, 8)
ellipse("Helmet_Faceplate", (0, -.138, 1.771), (.072, .022, .068),
        reinforce, "neck", 20, 10)
box("Helmet_Nasal_Bridge", (0, -.167, 1.807), (.011, .007, .047),
    metal, "neck", .004)
ellipse("Helmet_Chin_Guard", (0, -.128, 1.713), (.085, .028, .025),
        metal, "neck", 16, 8)
box("Helmet_Neck_Seal", (0, 0, 1.670), (.115, .105, .039),
    rubber, "neck", .011)

# Torso armor floats just above the graphite jacket, leaving fabric visible at
# the waist, underarms, sleeves, and leg joints.
ellipse("Chest_Backbone_Plate", (0, -.174, 1.426), (.178, .040, .124),
        reinforce, "chest", 24, 12)
plate("Chest_Diagonal_Orange", [(-.119, 1.539), (.119, 1.539),
      (.105, 1.437), (-.018, 1.404), (-.121, 1.455)], -.207, .011,
      paint, "chest")
plate("Chest_Diagonal_Dark_Cut", [(-.117, 1.437), (-.018, 1.405),
      (.107, 1.426), (.106, 1.413), (-.014, 1.389), (-.121, 1.421)],
      -.215, .005, reinforce, "chest")
box("Chest_Center_Buckle", (0, -.221, 1.391), (.065, .017, .033),
    metal, "chest", .005)
for x in (-.087, .087):
    bolt(f"Chest_Orange_Plate_Bolt.{x}", (x, -.225, 1.489), "chest", radius=.007)
for side, sign in (("L", 1), ("R", -1)):
    ellipse(f"Chest_Outer_Bib.{side}", (sign*.207, -.151, 1.425),
        (.066, .044, .103), metal, "chest", 16, 8)
    box(f"Chest_Shoulder_Strap.{side}", (sign*.204, -.157, 1.523),
        (.035, .030, .095), leather, "chest", .008)
    box(f"Chest_Inner_Harness.{side}", (sign*.126, -.181, 1.329),
        (.017, .016, .099), leather, "chest", .005)
    box(f"Chest_Strap_Buckle.{side}", (sign*.204, -.190, 1.516),
        (.043, .006, .023), metal, "chest", .002)
    box(f"Chest_Lower_Pouch.{side}", (sign*.176, -.198, 1.263),
        (.056, .028, .067), leather, "spine", .014)
    box(f"Chest_Lower_Pouch_Flap.{side}", (sign*.176, -.247, 1.296),
        (.061, .007, .025), metal, "spine", .003)
    box(f"Chest_Cyan_Status.{side}", (sign*.159, -.204, 1.565),
        (.014, .006, .013), cyan, "chest", .002)
    bolt(f"Chest_Fastener.{side}", (sign*.224, -.203, 1.472), "chest")
    for z in (1.285, 1.333):
        box(f"Chest_Harness_Stitch.{side}.{z}", (sign*.126, -.200, z),
            (.015, .003, .004), trim, "chest", .001)
box("Chest_Medical_Pouch", (0, -.202, 1.241), (.069, .026, .052),
    leather, "spine", .013)
box("Chest_Medical_Plate", (0, -.247, 1.245), (.057, .007, .030),
    metal, "spine", .003)

# Cylindrical rear module and paired small canisters, seen from both sides.
box("Back_Module_Base", (0, .176, 1.392), (.230, .037, .195),
    reinforce, "chest", .012)
box("Back_Module_Center_Armor", (0, .223, 1.435), (.100, .025, .116),
    metal, "chest", .008)
disc("Back_Module_Circular_Core", (0, .257, 1.393), .057, .022,
     paint, "chest", "Y", 18)
disc("Back_Module_Circular_Node", (0, .272, 1.393), .030, .013,
     reinforce, "chest", "Y", 18)
for z in (1.485, 1.439, 1.347, 1.300):
    box(f"Back_Module_Vent.{z}", (0, .259, z),
        (.077, .007, .008), reinforce, "chest", .002)
for side, sign in (("L", 1), ("R", -1)):
    box(f"Back_Module_Upper_Cyan.{side}", (sign*.131, .229, 1.519),
        (.020, .008, .029), cyan, "chest", .003)
    disc(f"Back_Module_Canister.{side}", (sign*.166, .236, 1.355),
         .038, .165, metal, "chest", "Z", 12)
    box(f"Back_Module_Lower_Hook.{side}", (sign*.158, .213, 1.230),
        (.045, .036, .037), leather, "spine", .005)
    box(f"Back_Module_Orange_Rail.{side}", (sign*.222, .200, 1.403),
        (.014, .026, .149), paint, "chest", .004)
    box(f"Back_Harness_Lower.{side}", (sign*.169, .177, 1.177),
        (.035, .018, .076), leather, "spine", .006)

# Belt, front/rear utility pouches, and the two asymmetrical stowed tools.
box("Belt_Front_Orange_Buckle", (0, -.188, 1.015),
    (.043, .010, .028), paint, "pelvis", .004)
for side, sign in (("L", 1), ("R", -1)):
    for i, x in enumerate((.095, .211)):
        box(f"Belt_Front_Pouch.{side}.{i}", (sign*x, -.202, 1.009),
            (.045, .035, .055), leather, "pelvis", .006)
        box(f"Belt_Front_Pouch_Flap.{side}.{i}", (sign*x, -.239, 1.034),
            (.045, .005, .019), metal, "pelvis", .002)
    for i, x in enumerate((.087, .190)):
        box(f"Belt_Back_Pouch.{side}.{i}", (sign*x, .202, 1.009),
            (.045, .032, .055), leather, "pelvis", .006)
        box(f"Belt_Back_Pouch_Flap.{side}.{i}", (sign*x, .236, 1.034),
            (.045, .005, .019), metal, "pelvis", .002)
box("Right_Holster_Hard_Shell", (-.302, -.024, .758),
    (.044, .051, .134), reinforce, "thigh.R", .010)
box("Right_Holster_Orange_Lock", (-.348, -.024, .803),
    (.009, .037, .050), paint, "thigh.R", .003)
box("Left_Knife_Sheath_Hard_Shell", (.296, -.025, .758),
    (.034, .041, .143), metal, "thigh.L", .008)
box("Left_Knife_Sheath_Orange_Mark", (.332, -.025, .805),
    (.008, .031, .052), paint, "thigh.L", .003)
# The newer four-view sheet places the grenade slightly right of center.
# Keep the legacy left-front socket name for binary compatibility, but hide
# its older placeholder in this review model and show the new right clip.
legacy_grenade = bpy.data.objects.get("Grenade_Clip_Left_Front")
if legacy_grenade is not None:
    legacy_grenade.hide_render = True
    legacy_grenade.hide_set(True)
ellipse("Grenade_Right_Front_Clip", (-.215, -.223, .981),
        (.039, .042, .048), metal, "pelvis", 12, 6)
box("Grenade_Right_Front_Orange_Mark", (-.215, -.260, .988),
    (.018, .008, .031), paint, "pelvis", .004)
for side, sign in (("L", 1), ("R", -1)):
    for z in (.825, .706):
        box(f"Thigh_Strap.{side}.{z}", (sign*.168, -.124, z),
            (.110, .012, .015), leather, f"thigh.{side}", .004)
        box(f"Thigh_Strap_Buckle.{side}.{z}", (sign*.268, -.130, z),
            (.016, .018, .018), metal, f"thigh.{side}", .003)
    box(f"Thigh_Cargo_Pocket.{side}", (sign*.242, -.095, .755),
        (.044, .027, .093), leather, f"thigh.{side}", .013)
    box(f"Thigh_Cargo_Flap.{side}", (sign*.242, -.124, .806),
        (.045, .008, .023), trim, f"thigh.{side}", .004)

# Each limb uses the existing articulated chain. Armor stays on one bone;
# joints remain graphite and can fold without a stiff shell across them.
for side, sign in (("L", 1), ("R", -1)):
    upper = f"upper_arm.{side}"
    forearm = f"forearm.{side}"
    shin = f"shin.{side}"
    foot = f"foot.{side}"
    elbow_pos = (sign*.431, -.020, 1.174)
    ellipse(f"Shoulder_Pad_Base.{side}", (sign*.310, -.010, 1.477),
            (.127, .108, .063), reinforce, f"clavicle.{side}", 16, 8)
    ellipse(f"Shoulder_Orange_Shell.{side}", (sign*.325, -.028, 1.512),
            (.113, .080, .038), paint, f"clavicle.{side}", 16, 8)
    ellipse(f"Shoulder_Front_Orange.{side}", (sign*.330, -.105, 1.475),
            (.105, .033, .053), paint, f"clavicle.{side}", 16, 8)
    ellipse(f"Upper_Arm_Plate.{side}", (sign*.375, -.079, 1.349),
            (.076, .029, .091), metal, upper, 16, 8)
    ellipse(f"Upper_Arm_Orange_Band.{side}", (sign*.405, -.073, 1.314),
            (.029, .029, .077), paint, upper, 16, 8)
    disc(f"Elbow_Joint_Base.{side}", (sign*.515, elbow_pos[1], elbow_pos[2]),
         .048, .026, reinforce, forearm, "X", 16)
    disc(f"Elbow_Joint_Hub.{side}", (sign*.531, elbow_pos[1], elbow_pos[2]),
         .022, .009, metal, forearm, "X", 16)
    ellipse(f"Forearm_Orange_Cuff.{side}", (sign*.486, -.156, 1.044),
        (.071, .035, .105), paint, forearm, 16, 8)
    ellipse(f"Forearm_Dark_Inner.{side}", (sign*.478, -.044, 1.045),
        (.066, .039, .088), metal, forearm, 16, 8)
    for z in (.976, 1.038):
        box(f"Forearm_Rib.{side}.{z}", (sign*.493, -.150, z),
            (.062, .006, .009), reinforce, forearm, .003)
    box(f"Glove_Backplate.{side}", (sign*.518, -.181, .828),
        (.054, .009, .050), metal, f"hand.{side}", .005)
    # Knee cover is tied to the lower leg; the fabric knee remains a hinge.
    disc(f"Knee_Side_Hinge.{side}", (sign*.275, -.042, .550),
         .047, .024, reinforce, shin, "X", 16)
    disc(f"Knee_Side_Hub.{side}", (sign*.291, -.042, .550),
         .024, .008, metal, shin, "X", 16)
    ellipse(f"Knee_Orange_Pad.{side}", (sign*.170, -.150, .555),
        (.096, .035, .075), paint, shin, 16, 8)
    box(f"Knee_Upper_Rim.{side}", (sign*.170, -.181, .600),
        (.080, .012, .014), metal, shin, .004)
    box(f"Shin_Armor_Spine.{side}", (sign*.170, -.122, .323),
        (.085, .034, .169), metal, shin, .010)
    plate(f"Shin_Orange_Front.{side}",
          [(sign*.112, .456), (sign*.213, .447), (sign*.216, .239),
           (sign*.174, .172), (sign*.119, .239)], -.174, .011,
          paint, shin)
    box(f"Shin_Outer_Brace.{side}", (sign*.258, -.050, .322),
        (.019, .035, .134), metal, shin, .008)
    for z in (.217, .369):
        box(f"Shin_Strap.{side}.{z}", (sign*.171, -.171, z),
            (.092, .010, .014), leather, shin, .003)
    disc(f"Ankle_Outer_Hinge.{side}", (sign*.278, -.052, .139),
         .038, .020, reinforce, foot, "X", 14)
    box(f"Boot_Orange_Ankle.{side}", (sign*.170, -.151, .128),
        (.081, .052, .039), paint, foot, .011)
    box(f"Boot_Instep_Armor.{side}", (sign*.170, -.208, .078),
        (.107, .116, .024), metal, foot, .011)
    box(f"Boot_Toe_Rim.{side}", (sign*.170, -.289, .061),
        (.108, .023, .022), reinforce, foot, .007)
    box(f"Boot_Orange_Toe_Clip.{side}", (sign*.170, -.291, .082),
        (.055, .018, .019), paint, foot, .005)
    bolt(f"Knee_Bolt.{side}", (sign*.170, -.191, .555), shin)
    bolt(f"Boot_Bolt.{side}", (sign*.170, -.326, .061), foot)

# Keep the six animation clips and six equipment sockets byte-for-byte in
# naming and object structure. Only the neck's deform flag is intentionally
# changed so the new helmet follows it.
assert tuple(b.name for b in rig.data.bones) == original_bones
assert tuple(sorted(a.name for a in bpy.data.actions)) == original_actions
assert tuple(sorted(o.name for o in bpy.data.objects
                    if o.name.startswith("SOCKET_"))) == original_sockets
assert all(len(v.groups) <= 2 for o in new_objects for v in o.data.vertices)
rig["source_reference_exosuit"] = "visual/Proposta_visual_separada/player/ (four views)"
rig["exosuit_status"] = "Blender review model; runtime integration separate"

# Restore the neutral/bind pose for all four model-sheet angles.
rig.animation_data.action = bpy.data.actions["Idle"]
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
rig.animation_data.action = None


def aim_bone(name, head, tail):
    pose_bone = rig.pose.bones[name]
    rest = pose_bone.bone
    rest_direction = rest.tail_local - rest.head_local
    direction = Vector(tail) - Vector(head)
    rotation = rest_direction.rotation_difference(direction)
    pose_bone.matrix = (Matrix.Translation(head) @ rotation.to_matrix().to_4x4()
                        @ Matrix.Translation(-rest.head_local) @ rest.matrix_local)
    bpy.context.view_layer.update()


# The source's relaxed bind pose lowers the arms. A review T pose is needed to
# compare all four silhouettes directly with the user's model sheet. It does
# not add a new animation action or alter the rest skeleton.
for side, sign in (("L", 1), ("R", -1)):
    shoulder = (sign*.29, 0, 1.48)
    elbow = (sign*.57, -.02, 1.47)
    wrist = (sign*.84, -.02, 1.465)
    fingertip = (sign*.96, -.02, 1.46)
    aim_bone(f"upper_arm.{side}", shoulder, elbow)
    aim_bone(f"forearm.{side}", elbow, wrist)
    aim_bone(f"hand.{side}", wrist, fingertip)

scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 12
scene.cycles.use_denoising = True
scene.render.resolution_x = 960
scene.render.resolution_y = 800
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.film_transparent = False
scene.view_settings.view_transform = "Standard"
scene.world.color = (.11, .11, .11)

# Reuse the source studio lights but give all four ortho angles the same key.
for obj in bpy.data.objects:
    if obj.type == "LIGHT":
        obj.data.energy *= 1.05
camera = bpy.data.objects.get("Review_Ortho")
if camera is None:
    raise RuntimeError("Source review camera missing")
scene.camera = camera
camera.data.type = "ORTHO"
camera.data.ortho_scale = 2.90
for name, location in (
    ("front", (0, -4.8, 1.05)),
    ("back", (0, 4.8, 1.05)),
    ("left", (4.8, 0, 1.05)),
    ("right", (-4.8, 0, 1.05)),
):
    camera.location = location
    camera.rotation_euler = (Vector((0, 0, 1.02)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = str(REVIEW / f"player_exosuit_{name}.png")
    bpy.ops.render.render(write_still=True)

scene.render.filepath = "//review.png"
# Keep the editable file neutral so users can select and scrub each existing
# action without an unkeyed T pose being mistaken for an animation clip.
rig.animation_data.action = bpy.data.actions["Idle"]
scene.frame_set(1)
for pose_bone in rig.pose.bones:
    pose_bone.rotation_euler = (0, 0, 0)
    pose_bone.location = (0, 0, 0)
bpy.context.view_layer.update()
bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.file.pack_all()
destination = OUT / "player_exosuit.blend"
bpy.ops.wm.save_as_mainfile(filepath=str(destination), compress=True)
assert SOURCE.exists() and destination.exists()
depsgraph = bpy.context.evaluated_depsgraph_get()
triangles = 0
for obj in bpy.data.objects:
    if obj.type == "MESH" and obj in new_objects:
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        mesh.calc_loop_triangles()
        triangles += len(mesh.loop_triangles)
        evaluated.to_mesh_clear()
print(f"EXOSUIT_OK blend={destination} added_objects={len(new_objects)} added_triangles={triangles} "
      f"bones={len(original_bones)} actions={len(original_actions)} sockets={len(original_sockets)}")

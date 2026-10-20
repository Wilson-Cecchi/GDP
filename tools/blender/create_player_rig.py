"""Build the editable Iron Seed player rig and technical previews in Blender 5.2.

Run: blender -b -t 2 --python tools/blender/create_player_rig.py
The output is deliberately separate from the runtime player implementation.
"""

import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


ROOT = Path(__file__).resolve().parents[2]
MODEL_DIR = ROOT / "assets/visual/models/player_rig"
PREVIEW_DIR = ROOT / "visual/validation/player_blender_rig"
MODEL_DIR.mkdir(parents=True, exist_ok=True)
PREVIEW_DIR.mkdir(parents=True, exist_ok=True)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)


def material(name, color, roughness=0.86, metallic=0.0):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    node = m.node_tree.nodes.get("Principled BSDF")
    node.inputs["Base Color"].default_value = (*color, 1)
    node.inputs["Roughness"].default_value = roughness
    node.inputs["Metallic"].default_value = metallic
    return m


cloth = material("M01_Graphite_Cloth", (0.031, 0.035, 0.039))
seam = material("M02_Stitched_Trim", (0.055, 0.057, 0.060))
reinforcement = material("M03_Reinforcement", (0.020, 0.022, 0.025))
leather = material("M04_Belt_Leather", (0.014, 0.016, 0.019))
metal = material("M05_Blackened_Metal", (0.08, 0.084, 0.088), 0.42, 0.58)
rubber = material("M06_Rubber", (0.010, 0.012, 0.014))

rig_data = bpy.data.armatures.new("Player_ArmatureData")
rig = bpy.data.objects.new("Player_Rig", rig_data)
bpy.context.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")


def bone(name, head, tail, parent=None, deform=True):
    b = rig_data.edit_bones.new(name)
    b.head, b.tail = head, tail
    b.use_deform = deform
    if parent:
        b.parent = rig_data.edit_bones[parent]
        b.use_connect = False
    return b


bone("root", (0, 0, 0), (0, 0, .16), deform=False)
bone("pelvis", (0, 0, .91), (0, 0, 1.08), "root")
bone("spine", (0, 0, 1.08), (0, 0, 1.36), "pelvis")
bone("chest", (0, 0, 1.36), (0, 0, 1.53), "spine")
bone("neck", (0, 0, 1.53), (0, 0, 1.72), "chest", False)
for side, sign in (("L", 1), ("R", -1)):
    bone(f"clavicle.{side}", (sign*.12, 0, 1.50), (sign*.29, 0, 1.48), "chest")
    bone(f"upper_arm.{side}", (sign*.29, 0, 1.48), (sign*.43, -.02, 1.17), f"clavicle.{side}")
    bone(f"forearm.{side}", (sign*.43, -.02, 1.17), (sign*.50, -.12, .91), f"upper_arm.{side}")
    bone(f"hand.{side}", (sign*.50, -.12, .91), (sign*.52, -.16, .79), f"forearm.{side}")
    bone(f"thigh.{side}", (sign*.145, 0, .96), (sign*.17, -.03, .55), "pelvis")
    bone(f"shin.{side}", (sign*.17, -.03, .55), (sign*.17, .005, .14), f"thigh.{side}")
    bone(f"foot.{side}", (sign*.17, .005, .14), (sign*.17, -.23, .08), f"shin.{side}")
bpy.ops.object.mode_set(mode="OBJECT")
rig.show_in_front = True
rig["coordinate_contract"] = "metres, Blender X right / -Y forward / Z up"
rig["source_reference"] = "visual/proposals/player_body_proposal_v4.png"
rig["runtime_status"] = "prototype only: no game import or first-person validation"


def skin(obj, weights):
    obj.parent = rig
    for name, value in weights.items():
        group = obj.vertex_groups.new(name=name)
        group.add(list(range(len(obj.data.vertices))), value, "REPLACE")
    mod = obj.modifiers.new("Armature_Deform", "ARMATURE")
    mod.object = rig
    mod.use_deform_preserve_volume = True


def ellipsoid(name, center, scale, mat, weights, segments=16, rings=8):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    for p in obj.data.polygons:
        p.use_smooth = True
    skin(obj, weights)
    return obj


def bevel_box(name, center, half_size, mat, weights, bevel=.018):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.scale = tuple(2*v for v in half_size)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bevel_modifier = obj.modifiers.new("Softened_Edges", "BEVEL")
    bevel_modifier.width = bevel
    bevel_modifier.segments = 3
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=bevel_modifier.name)
    obj.data.materials.append(mat)
    skin(obj, weights)
    return obj


def torso_loft():
    rows = [(.96, .205, .125), (1.06, .255, .155), (1.20, .235, .145),
            (1.36, .295, .175), (1.48, .325, .185), (1.60, .245, .14),
            (1.66, .13, .105)]
    verts, faces = [], []
    for z, rx, ry in rows:
        for j in range(20):
            a = 2*math.pi*j/20
            verts.append((rx*math.cos(a), ry*math.sin(a), z))
    for i in range(len(rows)-1):
        for j in range(20):
            a, b = i*20+j, i*20+(j+1)%20
            faces.append((a, b, b+20, a+20))
    # Bottom closes into trousers; top stays open as on the neck-down reference.
    faces.append(tuple(reversed(range(20))))
    mesh = bpy.data.meshes.new("Jacket_OnePiece_Mesh")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new("Jacket_OnePiece", mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(cloth)
    obj.parent = rig
    groups = {name: obj.vertex_groups.new(name=name) for name in ("pelvis", "spine", "chest")}
    for i in range(len(rows)):
        z = rows[i][0]
        if z < 1.1:
            weights = {"pelvis": 1}
        elif z < 1.4:
            weights = {"spine": 1}
        else:
            weights = {"chest": 1}
        for name, value in weights.items():
            groups[name].add(list(range(i*20, (i+1)*20)), value, "REPLACE")
    mod = obj.modifiers.new("Armature_Deform", "ARMATURE")
    mod.object = rig
    mod.use_deform_preserve_volume = True
    for p in mesh.polygons:
        p.use_smooth = True
    return obj


def limb(name, points, radii, bone_a, bone_b, mat, sides=12):
    verts, faces = [], []
    for i, point in enumerate(points):
        p = Vector(point)
        direction = Vector(points[min(i+1, len(points)-1)]) - Vector(points[max(0, i-1)])
        direction.normalize()
        u = direction.cross(Vector((0, 1, 0))).normalized()
        v = direction.cross(u).normalized()
        for j in range(sides):
            theta = 2*math.pi*j/sides
            verts.append(p + radii[i]*(math.cos(theta)*u + math.sin(theta)*v))
    for i in range(len(points)-1):
        for j in range(sides):
            a, b = i*sides+j, i*sides+(j+1)%sides
            faces.append((a, b, b+sides, a+sides))
    faces.extend((tuple(reversed(range(sides))), tuple((len(points)-1)*sides+j for j in range(sides))))
    mesh = bpy.data.meshes.new(name+"_Mesh")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(mat)
    obj.parent = rig
    upper = obj.vertex_groups.new(name=bone_a)
    lower = obj.vertex_groups.new(name=bone_b)
    # Mid-joint rings blend continuously; elbow/knee remain anatomically placed.
    blend = (0, .08, .5, .92, 1)
    for i, weight in enumerate(blend):
        ids = list(range(i*sides, (i+1)*sides))
        if weight < 1:
            upper.add(ids, 1-weight, "REPLACE")
        if weight > 0:
            lower.add(ids, weight, "REPLACE")
    mod = obj.modifiers.new("Armature_Deform", "ARMATURE")
    mod.object = rig
    mod.use_deform_preserve_volume = True
    for p in mesh.polygons:
        p.use_smooth = True
    return obj


# Clothing silhouette follows the approved *direction* of v4, not a texture bake.
torso_loft()
bpy.ops.mesh.primitive_torus_add(major_radius=.112, minor_radius=.012, location=(0, 0, 1.655))
collar = bpy.context.object
collar.name = "Jacket_Open_Collar"
collar.scale = (1.18, .95, 1)
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
collar.data.materials.append(reinforcement)
skin(collar, {"chest": 1})
for sign in (-1, 1):
    ellipsoid(f"Shoulder_Seam_{sign}", (sign*.245, -.108, 1.52), (.105, .012, .012),
              seam, {"chest": 1}, 12, 6)
ellipsoid("Jacket_CenterZip", (0, -.177, 1.40), (.004, .005, .225), metal, {"chest": 1}, 8, 6)
bevel_box("Belt_Buckle", (0, -.167, 1.015), (.043, .012, .031), metal, {"pelvis": 1}, .005)
for sign in (-1, 1):
    bevel_box(f"Belt_Front_{sign}", (sign*.135, -.158, 1.015), (.091, .014, .028), leather,
              {"pelvis": 1}, .012)
    bevel_box(f"Belt_Side_{sign}", (sign*.257, 0, 1.015), (.022, .126, .028), leather,
              {"pelvis": 1}, .01)
bevel_box("Belt_Back", (0, .15, 1.015), (.218, .014, .028), leather, {"pelvis": 1}, .01)

for side, sign in (("L", 1), ("R", -1)):
    a = (sign*.29, 0, 1.48)
    e = (sign*.43, -.02, 1.17)
    w = (sign*.50, -.12, .91)
    limb(f"Jacket_Sleeve.{side}", [a, (sign*.355, -.01, 1.335), e,
         (sign*.475, -.07, 1.045), w], [.108, .106, .085, .075, .066],
         f"upper_arm.{side}", f"forearm.{side}", cloth)
    bevel_box(f"Glove_Palm.{side}", (sign*.52, -.145, .83), (.055, .033, .057), rubber,
              {f"hand.{side}": 1}, .018)
    ellipsoid(f"Glove_Cuff.{side}", (sign*.5, -.12, .905), (.064, .06, .04),
              reinforcement, {f"hand.{side}": 1}, 12, 6)
    for finger in range(4):
        finger_x = sign*(.479 + finger*.026)
        ellipsoid(f"Glove_Finger{finger}.{side}", (finger_x, -.15, .766),
                  (.011, .014, .041), rubber, {f"hand.{side}": 1}, 8, 6)
    ellipsoid(f"Glove_Thumb.{side}", (sign*.582, -.155, .815), (.019, .018, .047),
              rubber, {f"hand.{side}": 1}, 8, 6)
    bevel_box(f"KneePad.{side}", (sign*.17, -.122, .55), (.092, .025, .07), reinforcement,
              {f"thigh.{side}": .45, f"shin.{side}": .55}, .017)
    hip, knee, ankle = (sign*.145, 0, .96), (sign*.17, -.03, .55), (sign*.17, .005, .14)
    limb(f"Trousers.{side}", [hip, (sign*.155, -.01, .755), knee,
         (sign*.17, -.01, .345), ankle], [.143, .138, .112, .103, .087],
         f"thigh.{side}", f"shin.{side}", cloth)
    bevel_box(f"Thigh_Pocket.{side}", (sign*.253, -.015, .79), (.018, .065, .095),
              cloth, {f"thigh.{side}": 1}, .012)
    bevel_box(f"Boot_Upper.{side}", (sign*.17, -.055, .105), (.092, .135, .078), leather,
              {f"foot.{side}": 1}, .04)
    bevel_box(f"Boot_Toe.{side}", (sign*.17, -.20, .075), (.105, .095, .052), leather,
              {f"foot.{side}": 1}, .033)
    bevel_box(f"Boot_Sole.{side}", (sign*.17, -.12, .022), (.108, .22, .022), rubber,
              {f"foot.{side}": 1}, .012)


# Retained items are visual placeholders, each deforming with the pelvis.
bevel_box("Holster_Pistol_Right", (-.28, -.015, .87), (.049, .048, .14), leather,
          {"pelvis": 1}, .018)
bevel_box("Sheath_Knife_Left", (.29, .015, .88), (.03, .03, .13), leather,
          {"pelvis": 1}, .014)
ellipsoid("Grenade_Clip_Left_Front", (.22, -.19, 1.01), (.048, .044, .055), metal,
          {"pelvis": 1}, 12, 6)
bevel_box("Primary_Back_Mount", (-.18, .175, 1.40), (.07, .02, .15), leather,
          {"chest": 1}, .015)


def socket(name, bone_name, location):
    obj = bpy.data.objects.new(name, None)
    bpy.context.collection.objects.link(obj)
    obj.empty_display_type = "ARROWS"
    obj.empty_display_size = .07
    obj.parent = rig
    obj.parent_type = "BONE"
    obj.parent_bone = bone_name
    # Set desired rest-pose world transform after bone parenting; Blender
    # calculates the local offset. A raw location would include the bone tail.
    obj.matrix_world = Matrix.Translation(location)
    obj["runtime_item_rule"] = "Show only if owned and stowed; hide when drawn"
    return obj


socket("SOCKET_PRIMARY_BACK", "chest", (-.18, .22, 1.40))
socket("SOCKET_PISTOL_RIGHT_HIP", "pelvis", (-.31, -.01, .90))
socket("SOCKET_KNIFE_LEFT_HIP", "pelvis", (.33, .01, .89))
socket("SOCKET_GRENADE_LEFT_FRONT", "pelvis", (.22, -.20, 1.02))
socket("SOCKET_GRIP.L", "hand.L", (.52, -.16, .78))
socket("SOCKET_GRIP.R", "hand.R", (-.52, -.16, .78))

POSE_BONES = [b.name for b in rig.pose.bones]
for b in rig.pose.bones:
    b.rotation_mode = "XYZ"


def aim_bone(name, head, tail):
    """Set a posed bone's armature-space direction, retaining its rest roll."""
    pose_bone = rig.pose.bones[name]
    rest = pose_bone.bone
    rest_direction = rest.tail_local-rest.head_local
    direction = Vector(tail)-Vector(head)
    rotation = rest_direction.rotation_difference(direction)
    pose_bone.matrix = (Matrix.Translation(head) @ rotation.to_matrix().to_4x4()
                        @ Matrix.Translation(-rest.head_local) @ rest.matrix_local)
    bpy.context.view_layer.update()


def right_arm_back_reach(reach):
    """Two-bone reach to the primary mount, bending the elbow above the shoulder."""
    if reach <= 0:
        return
    shoulder = Vector(rig.data.bones["upper_arm.R"].head_local)
    rest_wrist = Vector(rig.data.bones["forearm.R"].tail_local)
    # The grip sits by the right shoulder on the back mount, allowing an
    # anatomically lateral elbow rather than pulling the arm across the neck.
    wrist = rest_wrist.lerp(Vector((-.31, .247, 1.43)), reach)
    upper = rig.data.bones["upper_arm.R"].length
    lower = rig.data.bones["forearm.R"].length
    to_wrist = wrist-shoulder
    distance = to_wrist.length
    direction = to_wrist.normalized()
    along = (upper*upper-lower*lower+distance*distance)/(2*distance)
    height = math.sqrt(max(0, upper*upper-along*along))
    midpoint = shoulder+direction*along
    pole = Vector((-.65, .32, 1.48))
    bend = pole-midpoint
    bend -= direction*bend.dot(direction)
    bend.normalize()
    elbow = midpoint+bend*height
    aim_bone("upper_arm.R", shoulder, elbow)
    aim_bone("forearm.R", elbow, wrist)
    # Preserve the hand's natural downward orientation. Its grip socket then
    # lands next to the mount while the elbow clears the shoulder.


def apply_pose(frame, mode, phase):
    for b in rig.pose.bones:
        b.rotation_euler = (0, 0, 0)
        b.location = (0, 0, 0)
    swing = math.sin(phase)
    if mode in ("walk", "jog"):
        gain = .48 if mode == "walk" else .72
        rig.pose.bones["pelvis"].location.z = abs(swing)*(.018 if mode == "walk" else .035)
        rig.pose.bones["spine"].rotation_euler.y = -.045 if mode == "jog" else -.015
        for side, s in (("L", 1), ("R", -1)):
            leg = swing*s
            rig.pose.bones[f"thigh.{side}"].rotation_euler.x = gain*leg
            rig.pose.bones[f"shin.{side}"].rotation_euler.x = max(0, -leg)*gain*.65
            rig.pose.bones[f"foot.{side}"].rotation_euler.x = -.25*leg
            rig.pose.bones[f"upper_arm.{side}"].rotation_euler.x = -.38*leg
            rig.pose.bones[f"forearm.{side}"].rotation_euler.x = -.12-.13*max(0, leg)
    elif mode == "crouch":
        rig.pose.bones["pelvis"].location.z = -.24
        rig.pose.bones["spine"].rotation_euler.x = .22
        for side in ("L", "R"):
            rig.pose.bones[f"thigh.{side}"].rotation_euler.x = -.69
            rig.pose.bones[f"shin.{side}"].rotation_euler.x = 1.30
            rig.pose.bones[f"foot.{side}"].rotation_euler.x = -.55
            rig.pose.bones[f"upper_arm.{side}"].rotation_euler.x = -.22
    elif mode == "jump":
        rig.pose.bones["root"].location.z = max(0, math.sin(phase))*.25
        for side in ("L", "R"):
            rig.pose.bones[f"thigh.{side}"].rotation_euler.x = -.27
            rig.pose.bones[f"shin.{side}"].rotation_euler.x = .42
            rig.pose.bones[f"upper_arm.{side}"].rotation_euler.x = -.30
    elif mode == "draw":
        reach = max(0, min(1, phase))
        right_arm_back_reach(reach)
        rig.pose.bones["upper_arm.L"].rotation_euler.x = -.30*reach
        rig.pose.bones["forearm.L"].rotation_euler.x = -.36*reach
    elif mode == "idle":
        rig.pose.bones["spine"].rotation_euler.x = .008*math.sin(phase)
        rig.pose.bones["upper_arm.L"].rotation_euler.x = .01*math.sin(phase)
        rig.pose.bones["upper_arm.R"].rotation_euler.x = .01*math.sin(phase)
    for name in POSE_BONES:
        p = rig.pose.bones[name]
        p.keyframe_insert(data_path="rotation_euler", frame=frame, group=name)
        p.keyframe_insert(data_path="location", frame=frame, group=name)


actions = {
    "Idle": ("idle", [(1, 0), (16, math.pi/2), (32, math.pi), (48, 3*math.pi/2), (64, 2*math.pi)]),
    "Walk": ("walk", [(1, 0), (9, math.pi/2), (17, math.pi), (25, 3*math.pi/2), (33, 2*math.pi)]),
    "Jog": ("jog", [(1, 0), (6, math.pi/2), (11, math.pi), (16, 3*math.pi/2), (21, 2*math.pi)]),
    "Crouch": ("crouch", [(1, 0), (10, 0)]),
    "Jump": ("jump", [(1, 0), (8, math.pi/3), (15, math.pi/2), (23, 2*math.pi/3), (30, math.pi)]),
    "Draw_Primary_Back": ("draw", [(1, 0), (8, .35), (18, 1), (27, 1), (36, 0)]),
}

rig.animation_data_create()
for action_name, (mode, keys) in actions.items():
    action = bpy.data.actions.new(action_name)
    action.use_fake_user = True  # Keep every clip in the saved .blend.
    rig.animation_data.action = action
    for frame, phase in keys:
        apply_pose(frame, mode, phase)
    rig.animation_data.action["purpose"] = "Blender preview, not a runtime animation contract"


def activate(action, frame):
    rig.animation_data.action = bpy.data.actions[action]
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()


world = bpy.context.scene.world
world.color = (.12, .12, .12)

def light(name, loc, power, size):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = power
    data.shape = "DISK"
    data.size = size
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    obj.location = loc
    direction = Vector((0, 0, 1)) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


light("Studio_Key", (3, -4, 5), 550, 3)
light("Studio_Fill", (-3, -2, 3), 300, 3)
light("Studio_Rim", (0, 3, 4), 650, 2)
cam_data = bpy.data.cameras.new("Review_Ortho")
cam = bpy.data.objects.new("Review_Ortho", cam_data)
bpy.context.collection.objects.link(cam)
cam.location = (3.3, -5.4, 2.1)
cam.rotation_euler = (Vector((0, 0, 1)) - cam.location).to_track_quat("-Z", "Y").to_euler()
cam_data.type = "ORTHO"
cam_data.ortho_scale = 2.65
bpy.context.scene.camera = cam
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 8
scene.render.resolution_x = 600
scene.render.resolution_y = 720
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.film_transparent = False
scene.view_settings.view_transform = "AgX"

if "--skip-renders" not in sys.argv:
    for name, frame in (("Idle", 1), ("Walk", 9), ("Jog", 6), ("Crouch", 10),
                        ("Jump", 15), ("Draw_Primary_Back", 18)):
        activate(name, frame)
        scene.render.filepath = str(PREVIEW_DIR / f"player_{name.lower()}.png")
        bpy.ops.render.render(write_still=True)

    activate("Idle", 1)
    for name, location in (("front", (0, -5.4, 1.7)), ("side", (5.4, 0, 1.7))):
        cam.location = location
        cam.rotation_euler = (Vector((0, 0, 1)) - cam.location).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = str(PREVIEW_DIR / f"player_{name}.png")
        bpy.ops.render.render(write_still=True)

activate("Idle", 1)
scene.frame_start, scene.frame_end = 1, 64
scene.render.filepath = "//preview.png"
bpy.context.view_layer.objects.active = rig
for obj in bpy.context.selected_objects:
    obj.select_set(False)
rig.select_set(True)
blend_file = ROOT / "assets/visual/models/player_rig.blend"
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(blend_file), compress=True)

# Export only the deforming model and rig; lights/camera are review-only.
bpy.ops.object.select_all(action="DESELECT")
for obj in bpy.data.objects:
    if obj == rig or obj.type == "MESH" or obj.name.startswith("SOCKET_"):
        obj.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.export_scene.gltf(filepath=str(MODEL_DIR / "player_rig.glb"),
                          export_format="GLB", use_selection=True,
                          export_animations=True, export_skins=True,
                          export_yup=True)
print(f"PLAYER_RIG_OK blend={blend_file} glb={MODEL_DIR / 'player_rig.glb'}")

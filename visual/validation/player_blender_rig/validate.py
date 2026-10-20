"""Run with blender -b assets/visual/models/player_rig.blend --python this_file."""

from pathlib import Path
import json
import struct

import bpy

root = Path(__file__).resolve().parents[3]
rig = bpy.data.objects.get("Player_Rig")
assert rig is not None and rig.type == "ARMATURE"
required_bones = {"root", "pelvis", "spine", "chest", "neck"}
for side in ("L", "R"):
    required_bones.update(f"{part}.{side}" for part in
                          ("clavicle", "upper_arm", "forearm", "hand", "thigh", "shin", "foot"))
assert required_bones.issubset(rig.data.bones.keys())
assert rig.data.bones["forearm.R"].parent.name == "upper_arm.R"
assert rig.data.bones["shin.L"].parent.name == "thigh.L"
assert all((b.tail_local-b.head_local).length > .05 for b in rig.data.bones)
required_actions = {"Idle", "Walk", "Jog", "Crouch", "Jump", "Draw_Primary_Back"}
assert required_actions.issubset(bpy.data.actions.keys())
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
assert len(meshes) >= 25, len(meshes)
for obj in meshes:
    assert obj.parent == rig, obj.name
    assert any(m.type == "ARMATURE" and m.object == rig for m in obj.modifiers), obj.name
    assert obj.vertex_groups, obj.name
    assert all(v.groups for v in obj.data.vertices), obj.name
    assert all(g.name in rig.data.bones for g in obj.vertex_groups), obj.name
for name in ("SOCKET_PRIMARY_BACK", "SOCKET_PISTOL_RIGHT_HIP", "SOCKET_KNIFE_LEFT_HIP",
             "SOCKET_GRENADE_LEFT_FRONT", "SOCKET_GRIP.L", "SOCKET_GRIP.R"):
    socket = bpy.data.objects[name]
    assert socket.parent == rig and socket.parent_type == "BONE", name
for action in required_actions:
    rig.animation_data.action = bpy.data.actions[action]
    bpy.context.scene.frame_set(10)
    bpy.context.view_layer.update()
    assert all(abs(v) < 10 for b in rig.pose.bones for v in b.matrix.translation), action

rig.animation_data.action = bpy.data.actions["Draw_Primary_Back"]
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
grip = bpy.data.objects["SOCKET_GRIP.R"]
mount = bpy.data.objects["SOCKET_PRIMARY_BACK"]
rest_grip = grip.matrix_world.translation.copy()
bpy.context.scene.frame_set(18)
bpy.context.view_layer.update()
reach_distance = (grip.matrix_world.translation-mount.matrix_world.translation).length
elbow = rig.matrix_world @ rig.pose.bones["forearm.R"].head
shoulder = rig.matrix_world @ rig.pose.bones["upper_arm.R"].head
assert reach_distance <= .12, f"draw grip/mount gap: {reach_distance:.3f} m"
assert elbow.x < shoulder.x - .16 and elbow.y > shoulder.y + .10, \
    "draw elbow is not lateral and behind the shoulder"
assert abs(elbow.z-shoulder.z) < .10, "draw elbow rises too far above the shoulder"
bpy.context.scene.frame_set(36)
bpy.context.view_layer.update()
return_distance = (grip.matrix_world.translation-rest_grip).length
assert return_distance <= .01, f"draw does not return to rest: {return_distance:.3f} m"
print(f"DRAW_REACH_OK: grip/mount={reach_distance:.3f}m return={return_distance:.3f}m")

glb = root / "assets/visual/models/player_rig/player_rig.glb"
blob = glb.read_bytes()
assert blob[:4] == b"glTF"
json_len, json_type = struct.unpack_from("<II", blob, 12)
assert json_type == 0x4E4F534A
document = json.loads(blob[20:20+json_len])
clip_names = {entry["name"] for entry in document.get("animations", [])}
assert required_actions == clip_names, clip_names
assert len(document.get("skins", [])) == 1

# GLB survives a clean import with skeleton, weighted mesh and clips.
original_actions = set(bpy.data.actions.keys())
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(glb))
armatures = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
assert len(armatures) == 1
imported = armatures[0]
assert required_bones.issubset(imported.data.bones.keys())
assert any(o.type == "MESH" for o in bpy.context.scene.objects)
imported_actions = [a for a in bpy.data.actions if a.name not in original_actions]
assert len(imported_actions) >= 6, [a.name for a in imported_actions]
walk = next(a for a in imported_actions if a.name.startswith("Walk"))
imported.animation_data_create()
imported.animation_data.action = walk
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
left_rest = imported.pose.bones["thigh.L"].matrix.copy()
bpy.context.scene.frame_set(9)
bpy.context.view_layer.update()
left_step = imported.pose.bones["thigh.L"].matrix.copy()
assert (left_step.to_quaternion().rotation_difference(left_rest.to_quaternion()).angle > .15), \
    "GLB walk clip does not rotate the left thigh"
draw = next(a for a in imported_actions if a.name.startswith("Draw_Primary_Back"))
imported.animation_data.action = draw
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
imported_rest_wrist = imported.pose.bones["hand.R"].head.copy()
bpy.context.scene.frame_set(18)
bpy.context.view_layer.update()
imported_peak_wrist = imported.pose.bones["hand.R"].head.copy()
assert (imported_peak_wrist-imported_rest_wrist).length > .35, \
    "reimported GLB draw clip does not move the right arm"
bpy.context.scene.frame_set(36)
bpy.context.view_layer.update()
assert (imported.pose.bones["hand.R"].head-imported_rest_wrist).length < .02, \
    "reimported GLB draw clip does not return to rest"
print("PLAYER_RIG_VALIDATION_OK: blend skeleton/weights/sockets/actions; six GLB clips; animated walk and draw-return")

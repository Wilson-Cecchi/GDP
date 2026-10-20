"""Render the corrected draw reach from front and rear three-quarter views."""

from pathlib import Path

import bpy
from mathutils import Vector

out = Path(__file__).resolve().parent
rig = bpy.data.objects["Player_Rig"]
rig.animation_data.action = bpy.data.actions["Draw_Primary_Back"]
bpy.context.scene.frame_set(18)
bpy.context.view_layer.update()
camera = bpy.context.scene.camera
for label, location in (("player_draw_primary_back", (-3.3, -5.4, 2.1)),
                        ("player_draw_rear", (-3.3, 5.4, 2.1))):
    camera.location = location
    camera.rotation_euler = (Vector((0, 0, 1)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.render.filepath = str(out / f"{label}.png")
    bpy.ops.render.render(write_still=True)

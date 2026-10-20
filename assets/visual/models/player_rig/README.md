# Player rig (Blender prototype)

`../player_rig.blend` is the editable Blender 5.2.2 source. `player_rig.glb` is a
technical interchange export. This is a rigged **blockout**, based on the v4
neck-down graphite uniform and equipment positions; it is not a game asset or
an approved visual replacement. The four-view illustration is a design
reference, not a texture or 3D model. The clothing still needs sculpted folds,
seams, fabric surface detail and final animation cleanup.

## Rebuild and inspect

```bash
blender -b -t 2 --python-exit-code 1 --python tools/blender/create_player_rig.py
blender -b --python-exit-code 1 assets/visual/models/player_rig.blend \
  --python visual/validation/player_blender_rig/validate.py
```

The generator writes eight review renders and `build.log` is captured by the
coordinator in `visual/validation/player_blender_rig/`. Open the `.blend`, choose
`Player_Rig`, select one of the six named actions in the Action Editor, and
scrub its frame range. Actions: `Idle`, `Walk`, `Jog`, `Crouch`, `Jump`, and
`Draw_Primary_Back`. The draw is an initial reach pose, not a complete sequence
of stowing one weapon and drawing another.

## Coordinate and skeleton contract

- Blender units are metres; feet at Z≈0, collar at Z≈1.66. Blender +X is the
  character's left in the front view, −Y is forward, +Z is up.
- The game currently uses +Y for up and −Z for forward. The glTF export uses
  its standard Y-up conversion. Runtime integration must explicitly verify
  orientation and winding when converting the exported skeleton to game space.
- Stable deform bones: `pelvis`, `spine`, `chest`, plus side suffix `.L`/`.R`
  for `clavicle`, `upper_arm`, `forearm`, `hand`, `thigh`, `shin`, `foot`.
  `root` and `neck` are controls. Each arm and leg has a named parent chain.
- Stable sockets: `SOCKET_PRIMARY_BACK` on `chest`, `SOCKET_PISTOL_RIGHT_HIP`,
  `SOCKET_KNIFE_LEFT_HIP`, `SOCKET_GRENADE_LEFT_FRONT` on `pelvis`, and
  `SOCKET_GRIP.L`/`.R` on the hands. The `R` and `L` sides refer to the
  character, not the viewer. The primary mount is behind the right shoulder
  at Blender `(−0.18, 0.22, 1.40)` so the elbow can stay lateral during the
  draw. Show equipment only while owned and stowed.
- The head is deliberately absent: the first-person camera must never see
  a self-intersecting head. A third-person head can be a separate later asset.

The runtime currently has a seven-bone lower-body presentation and a separate
first-person hand rig. This `.blend` does not replace either implementation.
Before game integration, retarget those systems, check grip/socket alignment,
run in-game captures at POTATO/MEDIUM and FOV 60/100, and obtain visual approval.

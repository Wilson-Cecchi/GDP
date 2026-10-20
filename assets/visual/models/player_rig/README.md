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

The runtime keeps its seven-bone lower-body presentation and separate
first-person hands by default. Set `GDP_PLAYER_RIG_PREVIEW=1` to display the
exported Blender body during a local playtest; see
`visual/validation/player_blender_game/README.md`. Final retargeting, grip
alignment and visual approval remain pending.

## Runtime preview export: `ISPLYR01`

This versioned binary is made from the saved `.blend` without changing it:

```bash
blender -b --python-exit-code 1 assets/visual/models/player_rig.blend \
  --python tools/blender/export_player_rig.py
python3 tools/blender/validate_player_rig_export.py
```

`player_rig.isplayer` is 690,196 bytes, with 19 bones, six material groups,
7,222 triangles and six animation clips. It is an **opt-in preview input**.
The export omits only `Jacket_CenterZip` (42 vertices, 80 triangles): this
thin metal detail appeared as a vertical stick in the first-person camera.
The editable `.blend` and interchange `.glb` still contain the piece. The
preview omission needs confirmation in a native game capture.
The existing `ISEMESH1` enemy format has rigid pivots
and cannot represent this weighted Player. The new file uses the existing
Player vertex shader's two-bone UV encoding and fits its 36-matrix uniform
array. The renderer loads and samples this file only when the preview flag is set.

All integers and floats are little-endian. Layout, without padding:

```text
8 bytes  ASCII "ISPLYR01"
u32      boneCount = 19
u32      meshCount = 6
u32      clipCount = 6
repeat meshCount:
  u32    materialId (0..5)
  u32    vertexCount
  u32    indexCount (multiple of 3)
  repeat vertexCount: float32 position[3], normal[3], encodedUv[2]
  repeat indexCount: u32 triangleIndex
repeat clipCount:
  char   name[32] (ASCII, NUL-padded)
  u32    frameCount (inclusive range)
  u32    startFrame
  repeat frameCount * boneCount: float32 skinMatrix[16], column-major
EOF exactly
```

Bone indices are fixed in this order:

| Index | Bone | Index | Bone | Index | Bone |
| ---: | --- | ---: | --- | ---: | --- |
| 0 | root | 1 | pelvis | 2 | spine |
| 3 | chest | 4 | neck | 5 | clavicle.L |
| 6 | upper_arm.L | 7 | forearm.L | 8 | hand.L |
| 9 | thigh.L | 10 | shin.L | 11 | foot.L |
| 12 | clavicle.R | 13 | upper_arm.R | 14 | forearm.R |
| 15 | hand.R | 16 | thigh.R | 17 | shin.R |
| 18 | foot.R | | | | |

Material IDs follow the six generator materials. The file contains only the
material ID; its colors are configured by the loader. Linear RGB values in
the Blender source are: 0 Cloth `(0.031, 0.035, 0.039)`, 1 Trim
`(0.055, 0.057, 0.060)`, 2 Reinforcement `(0.020, 0.022, 0.025)`,
3 Leather `(0.014, 0.016, 0.019)`, 4 Metal `(0.080, 0.084, 0.088)`,
5 Rubber `(0.010, 0.012, 0.014)`. No albedo or normal atlas is embedded.

The export converts Blender position/normal `(x,y,z)` to game model space
`(x,z,y)`: +Y up and −Z forward. This conversion reflects the mesh, so
triangle winding is reversed. `encodedUv.x = 2*bone0 + weight0` and
`encodedUv.y = bone1`; the second weight is `1 − weight0`. A vertex with one
influence repeats its bone in both fields. The exporter rejects any vertex
with more than two influences. The current shader generates its own cloth
texture coordinates from bind position when `uPlayerSkinning=1`.

Each matrix is `C * (poseBone.matrix * inverse(restBone.matrix_local)) * C⁻¹`,
where `C` swaps Y and Z. Indices correspond to the bone order above. Clip
order is `Idle`, `Walk`, `Jog`, `Crouch`, `Jump`, `Draw_Primary_Back`. Source poses
were baked at the Blender scene rate of **24 frames/s**; the preview runtime
maps gait phase and equip progress onto those inclusive frame ranges. `Idle`
frame 1 is the bind pose (all identity skin matrices).
Other clips may start posed, so the sampler should blend from the previous
state rather than assuming their first frame is identity.

The independent Python reader verifies counts, finite data, weight encoding,
all indices, 7,222 nondegenerate front-facing triangles, all matrix values,
the idle bind frame and exact end of file. This does not validate on-screen
skin quality or GPU budget; those remain integration/playtest checks.

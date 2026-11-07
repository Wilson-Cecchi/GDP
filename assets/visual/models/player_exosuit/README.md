# Player tactical exosuit: Blender review model

`player_exosuit.blend` is an editable copy of `../player_rig.blend` with a
separate `Player_Exosuit_Armor` collection. The original rig file was not
changed. The new armor is attached to the existing 19-bone skeleton; all six
actions and six named equipment sockets remain present. `neck` is a deform
bone in this copy so the helmet follows head turns.

This model is a **stylized modular reconstruction for visual review** of the
four PNGs in `visual/Proposta_visual_separada/player/`. It is not a geometric
replica of the image. The source PNGs contain no mesh, UV layout, or separate
texture data. Fine cloth folds, detailed glove fingers, individually placed
paint chips, and the layered mechanical density of the illustrations still
need manual modeling and texturing if exact fidelity is required.

The model adds 171 mesh objects and 13,916 evaluated triangles. Together with
the source rig, the Blender scene contains 214 meshes and 21,218 triangles.
Each mesh vertex has at most two bone weights. Orange plates use the packed
`worker_paint_albedo.png` image, and the eight material names are M01 through
M08. `Helmet_` and `Visor_` names identify the 26 head meshes for exclusion
from the first-person camera. The game loads this asset only when
`GDP_PLAYER_EXOSUIT_PREVIEW=1` is set.

The newer four-view sheet puts the grenade slightly to the character's right.
The original `SOCKET_GRENADE_LEFT_FRONT` name remains for compatibility, while
the old visible left-front placeholder is hidden in this review model and a
right-front clip is shown. Runtime equipment placement needs an explicit
decision during integration.

To regenerate from a clean checkout, run from the repository root:

```bash
blender -b -t 2 --python-exit-code 1 assets/visual/models/player_rig.blend \
  --python tools/blender/create_player_exosuit.py
```

The script deliberately refuses to overwrite an existing `player_exosuit.blend`
so manual edits are not lost. Move an existing output aside before rerunning.
It also writes front, back, left, and right T-pose renders to
`visual/validation/player_exosuit_blender/` and saves the Blender source in
the relaxed Idle pose with its original animation actions.

## Runtime preview export

`player_exosuit.isplayer` is a separate `ISPLYR04` preview artifact. Generate
and validate it without saving either Blender source:

```bash
blender -b -t 2 --python-exit-code 1 \
  assets/visual/models/player_exosuit/player_exosuit.blend \
  --python tools/blender/export_player_exosuit.py
python3 tools/blender/validate_player_exosuit_export.py --self-test
```

It has 19 bones, 40 mesh groups (five parts × eight materials), six original
clips, 25,404 vertices, 19,626 triangles, and 1,285,084 bytes. Group order is
torso, legs, character-left arm, character-right arm, then world-only geometry;
each part has material IDs M01–M08 in that order. The last part contains all
`Helmet_`/`Visor_` meshes plus upper-chest and shoulder plates that otherwise
cross the first-person camera. The game currently has no third-person player
view, so this part is retained in the export for future use and excluded from
the FPS pass. The legacy collar, center zipper, shoulder seams, and left-front
grenade placeholder are omitted only from this export.

Vertex `UV` fields retain the existing two-bone skin encoding, so this binary
cannot carry the Blender paint UVs. The preview renderer samples the existing
Worker paint image for M07 with projected coordinates and gives M08 cyan
emission; paint wear placement therefore differs from the Blender source.

## Test in the game

```bash
cd /home/wilson/GDP/GDP-3
cmake -S . -B build/playtest -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DGDP_ENABLE_TEST_MODE=ON -DGDP_BUILD_TESTS=ON
cmake --build build/playtest --target gdp --parallel 4
GDP_PLAYER_EXOSUIT_PREVIEW=1 __NV_PRIME_RENDER_OFFLOAD=1 \
  __GLX_VENDOR_LIBRARY_NAME=nvidia ./build/playtest/gdp --test-mode
```

Omit the preview variable to compare with the normal body. The helmet is
visible in the Blender review renders but hidden from the first-person camera;
the visor-style HUD is a separate visual proposal. Automated OpenGL captures
are in `visual/validation/player_exosuit_game_v2/`. Visual acceptance in a
played match remains with the user.

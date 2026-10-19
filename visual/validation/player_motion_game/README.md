# Player — animação v1, capturas técnicas

Capturas do renderer real em cenário fixo, com a NVIDIA Quadro K2100M.
`standing`, `walking`, `gait_06`, `gait_18`, `crouched` e `airborne` mostram o
corpo inferior em MEDIUM/FOV 60. `walking_320x200_fov100_potato` cobre um
enquadramento compacto em POTATO. `equip_stow_pistol`, `equip_exchange`,
`equip_draw_carbine` e `draw_*` verificam a ordem e os pontos de saque. Os PNGs foram convertidos sem retoque dos
BMPs da fixture `--player-body-only`.

O torso/cinto v4 não está nesta build; sua [prancha](../../proposals/player_body_proposal_v4.md)
aguarda aprovação. Estas imagens comprovam renderização e mudança de pose,
mas não equivalem ao movimento percebido em partida ou à aprovação visual.

Regeneração:

```bash
cmake --build --preset release --target gdp_renderer_gl_tests --parallel 4
GDP_PLAYER_BODY_CAPTURE_DIR=build/player-motion-capture \
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
./build/release/gdp_renderer_gl_tests --player-body-only
```

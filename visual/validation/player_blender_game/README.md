# Player rig Blender — prévia na build

O modelo `assets/visual/models/player_rig.blend` foi exportado para
`player_rig.isplayer` e carregado pelo renderer com 19 ossos, 24 grupos de
material e os clips Idle, Walk, Jog, Crouch, Jump e Draw_Primary_Back. Esta
é uma **prévia opt-in**, não a aparência padrão aprovada. As mãos e armas em
primeira pessoa usam os contatos existentes; os braços visíveis vêm do Blender
na prévia. O torso, pernas e braços são separados no export. A inclinação do
olhar move o controle do pescoço e inclina levemente o peito. O `.blend` ainda
não tem malha de cabeça. Zíper, colar e duas costuras do ombro continuam no
`.blend`, mas são omitidos da prévia por cruzarem a câmera.

Para testar na sala de testes na NVIDIA:

```bash
cd /home/wilson/GDP/GDP-3
cmake -S . -B build/playtest -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DGDP_ENABLE_TEST_MODE=ON -DGDP_BUILD_TESTS=OFF
cmake --build build/playtest --target gdp --parallel 4
GDP_PLAYER_RIG_PREVIEW=1 __NV_PRIME_RENDER_OFFLOAD=1 \
  __GLX_VENDOR_LIBRARY_NAME=nvidia ./build/playtest/gdp --test-mode
```

Remova `GDP_PLAYER_RIG_PREVIEW=1` do último comando para comparar com o corpo
atual. A prévia é carregada do arquivo `.isplayer` copiado para
`build/playtest/visual/models/player_rig/`; edições no `.blend` exigem
reexportar com `tools/blender/export_player_rig.py` e recompilar a build.

As capturas abaixo foram atualizadas em 2026-10-20 a partir da fixture OpenGL
real na Quadro K2100M. Foram convertidas de BMP para PNG sem retoque.
MEDIUM/FOV 60 mostra a pose olhando
para baixo, caminhando, agachado e com carabina; POTATO/FOV 100 cobre a
silhueta com campo de visão amplo. `level` registra o horizonte. A fixture
compara pixels e contagem de draws/triângulos, mas não substitui a avaliação
visual e da movimentação em partida.

- [Parado, MEDIUM/FOV 60](standing_1280x720_fov60_medium.png)
- [Olhar intermediário, MEDIUM/FOV 60](look_mid_1280x720_fov60_medium.png)
- [Caminhando, MEDIUM/FOV 60](walking_1280x720_fov60_medium.png)
- [Agachado, MEDIUM/FOV 60](crouched_1280x720_fov60_medium.png)
- [Com carabina, MEDIUM/FOV 60](armed_1280x720_fov60_medium.png)
- [Com granada, MEDIUM/FOV 60](draw_grenade_1280x720_fov60_medium.png)
- [Horizonte, MEDIUM/FOV 60](level_1280x720_fov60_medium.png)
- [Parado, POTATO/FOV 100](standing_1280x720_fov100_potato.png)

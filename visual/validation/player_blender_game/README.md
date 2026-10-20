# Player rig Blender — prévia na build

O modelo `assets/visual/models/player_rig.blend` foi exportado para
`player_rig.isplayer` e carregado pelo renderer com 19 ossos, seis grupos de
material e os clips Idle, Walk, Jog, Crouch, Jump e Draw_Primary_Back. Esta
é uma **prévia opt-in**, não a aparência padrão aprovada. As mãos e armas em
primeira pessoa continuam no viewmodel existente. O modelo, materiais simples
e poses ainda precisam de revisão visual durante uma partida.
O detalhe `Jacket_CenterZip` permanece no `.blend`, mas foi omitido somente na
prévia de primeira pessoa: sua projeção criava um palito vertical no centro.
As capturas finais mostram o centro livre.

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

As capturas abaixo vêm da fixture OpenGL real na Quadro K2100M. Foram
convertidas de BMP para PNG sem retoque. MEDIUM/FOV 60 mostra a pose olhando
para baixo, caminhando, agachado e com carabina; POTATO/FOV 100 cobre a
silhueta com campo de visão amplo. `level` registra o horizonte. A fixture
compara pixels e contagem de draws/triângulos, mas não substitui a avaliação
visual e da movimentação em partida.

- [Parado, MEDIUM/FOV 60](standing_1280x720_fov60_medium.png)
- [Caminhando, MEDIUM/FOV 60](walking_1280x720_fov60_medium.png)
- [Agachado, MEDIUM/FOV 60](crouched_1280x720_fov60_medium.png)
- [Com carabina, MEDIUM/FOV 60](armed_1280x720_fov60_medium.png)
- [Horizonte, MEDIUM/FOV 60](level_1280x720_fov60_medium.png)
- [Parado, POTATO/FOV 100](standing_1280x720_fov100_potato.png)

# Terminal de missão — proposta A

O jogo carrega `terminal_a.isemesh` para o terminal da campanha e para a saída
do Boss. `terminal_a.obj` e `terminal_a.mtl` podem ser importados no Blender
para inspeção ou servir de base para edição. A fonte reproduzível é
`tools/visual/build_mission_terminal.py`; execute
`python3 tools/visual/build_mission_terminal.py` na raiz do repositório para
reconstruir os três arquivos. Edições feitas diretamente no OBJ ou no Blender
não entram no jogo automaticamente: exigem exportação para `ISEMESH1`.

O modelo mede 1,15 × 1,80 × 0,60 m e tem materiais para pintura, grafite,
metal exposto, juntas, visor e luzes. O vidro e as luzes recebem cores no
renderer conforme o estado da missão. A colisão de movimento e disparos fica
em `gameplay/Props.cpp`, dividida nas duas colunas, nas travessas e no corpo da
tela. Ao alterar a geometria, alinhe esses volumes e atualize os testes.

Captura técnica: `visual/validation/mission_terminal_a/terminal_a_ultra.png`.
Ela registra a malha nativa em Ultra. O usuário aprovou a aparência em partida
em 19/11/2026.

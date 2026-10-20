# Player Blender rig — inspeção técnica

Referência: `visual/proposals/player_body_proposal_v4.png`. Gerador:
`tools/blender/create_player_rig.py`. Fonte: `assets/visual/models/player_rig.blend`.
Exportação experimental: `assets/visual/models/player_rig/player_rig.glb`.

## Escopo comprovável

O protótipo possui armature hierárquica, malhas parentadas com grupos de
vértices e modificador Armature, seis ações separadas e seis sockets de
equipamento. **Validação headless passou**: o `.blend` foi reaberto; todos os
ossos, pesos, sockets e ações estão presentes. O GLB foi reimportado com um
skin e seis clips; no clip `Walk`, a coxa esquerda gira mais de 0,15 rad entre
os frames 1 e 9. No `Draw_Primary_Back` a luva direita termina a 0,070 m do
suporte atrás do ombro direito no frame 18 e retorna a 0,000 m da pose inicial
no frame 36. O cotovelo está lateral e atrás do ombro, quase na mesma altura.
O GLB reimportado preserva o movimento de ida e o retorno. Os sockets em
repouso coincidem com suas posições desejadas.
As capturas `player_front.png`, `player_side.png`, `player_walk.png`,
`player_crouch.png`, `player_draw_primary_back.png` e `player_draw_rear.png`
permitem inspecionar pose e
volume. `build.log` registra a reconstrução rápida final do mesmo modelo;
`validation.log` é o
registro de reabertura e reimportação. A validação mecânica automatizada não
substitui inspeção visual em jogo.

## Diferenças conhecidas da prancha v4

- O uniforme é um blockout procedural: faltam tecido esculpido, desgaste,
  textura, costuras e silhueta orgânica dos cotovelos, joelhos e mãos.
- Mangas e pernas ainda têm perfis tubulares, embora as juntas tenham ossos e
  pesos; os cotovelos e joelhos precisam de revisão de malha em poses extremas.
- A aparência ainda é monocromática nas capturas de estúdio; materiais e luzes
  não reproduzem as sutilezas de grafite e couro da prancha.
- Pistola, faca, granada e suporte de costas são volumes de posição; os modelos
  finais devem ser ligados aos sockets e ocultados conforme inventário.
- `Draw_Primary_Back` mostra o alcance ao suporte direito das costas e retorno.
  A captura frontal esconde a mão atrás do ombro; a traseira mostra o contato.
  A manga e a região do cotovelo ainda precisam de volume orgânico e revisão de
  colisão fina em todas as poses. Não há ainda
  transição completa de guardar a arma anterior, agarrar a nova e firmar a
  empunhadura. Nem o modelo nem as animações foram testados na build.
- Não há cabeça visível por design para a câmera em primeira pessoa.

Status: **blockout técnico; aprovação visual pendente**. Integrar apenas depois
de revisão artística, retarget do runtime e playtest em POTATO/MEDIUM com FOV
60/100, olhando também para baixo.

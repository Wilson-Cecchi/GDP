# Próxima fase visual: arsenal e Player

Decisão de 15/11/2026. Iniciar esta fase **depois** de concluir as variantes de
sala planejadas, aprovar seus elementos em partida e integrá-las à geração da
campanha. O critério de sala pronta está em
[`room_shape_plan.md`](../06_environments/room_shape_plan.md).

## 1. Propostas para todo o arsenal

Inventariar novamente os modelos de armas visíveis no jogo quando a fase
começar, incluindo armamentos dos inimigos e do Boss. O conjunto atual do
Player é carabina, submetralhadora, sniper, escopeta, pistola, faca e granada.
Usar [`Armas.md`](Proposta_visual_separada/Armas.md) e os modelos existentes
como referências, sem limitar novas propostas à paleta atual. Apresentar para
cada arma uma prancha com vistas frontal, lateral, superior e traseira,
proporções, materiais, desgaste, partes móveis e como ela aparece na mão,
guardada no corpo e como drop. Validar legibilidade em FOVs e perfis gráficos
distintos. O usuário aprova a proposta antes da modelagem definitiva.

## 2. Nova metralhadora pesada

Propor uma arma primária nova, obtida como **drop raro**, de **alto dano** e
**peso perceptível**. A proposta deve explicar sua silhueta, alimentação,
empunhadura, recuo, recarga e efeitos de tiro, além de como o peso afeta
movimento, saque e mira. Definir os números de dano, cadência, capacidade,
raridade e penalidades com comparação às armas existentes e playtest; evitar
que o dano alto elimine as escolhas de sniper, escopeta e carabina. Incluir
distribuição do drop e persistência em checkpoint/campanha na validação
funcional. Não adicionar a arma ao jogo antes da aprovação da proposta.

## 3. Modelagem e integração

Após a aprovação, produzir fontes Blender e modelos exportados correspondentes
às pranchas aprovadas, com medidas e pontos de contato definidos. Integrar
visões em primeira e terceira pessoa, arma no chão, animações de saque,
disparo, recarga e guarda, colisão quando aplicável, áudio e HUD. Comparar
capturas do jogo às vistas aprovadas e fazer playtest em campanha antes de
considerar cada arma concluída. Manter a linha de arte industrial do jogo.

## 4. Personagem do Player

Depois da fase de armas, modelar a versão definitiva do Player e ajustar o
esqueleto, mãos, pontos de ancoragem e animações para todos os equipamentos
aprovados. Validar corpo inteiro, visão em primeira pessoa e transições entre
armas, movimento e postura.

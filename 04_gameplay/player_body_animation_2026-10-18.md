# Player — refinamento de animação visual

Data: 2026-10-18. Estado: **IMPLEMENTADO EM VALIDAÇÃO**.

O corpo inferior usa a distância real pós-colisão para avançar o ciclo.
Movimento muito lento reduz a amplitude da passada; parar contra parede
reduz o peso até repouso. O quadril sobe discretamente a cada apoio, o pé
ganha altura suave no balanço e a ponta inclina antes de voltar ao chão.
Recuo, deslocamento lateral, agachamento e salto continuam no mesmo rig IK.

Arma e mãos recebem fase e peso do mesmo histórico visual. Um balanço pequeno
é aplicado à raiz comum do passe próximo, preservando o contato de ambas as
mãos com arma e mecanismo. O balanço diminui ao mirar, agachar ou sair do
chão. Não altera a câmera, a origem lógica do tiro, dano, munição ou
movimento físico.

Malha, mapas, 2096 triângulos e quatro draws do corpo permanecem iguais.
Testes CPU cobrem velocidade, direções, transições, IK, valores inválidos e
resposta do conjunto arma/mãos. A fixture OpenGL e suas
[capturas](../visual/validation/player_motion_game/README.md) verificam a
renderização; naturalidade em movimento depende de playtest humano.

O torso/cinto melhor trabalhado está em
[proposta v4](../visual/proposals/player_body_proposal_v4.md) e não faz parte
desta implementação.

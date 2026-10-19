# Boss — rajada progressiva e cobertura quebrável

Data: 2026-10-18. Estado: **RAJADA APROVADA EM PLAYTEST** em 2026-10-19.
O feedback foi "rajada do boss esta boa"; a aprovação cobre a cadência,
recarga e dano percebidos na rajada, sem validar separadamente a destruição
de cobertura.

O IndustrialRobot conserva a janela inicial de reação e o bloqueio de tiro
quando um aliado ocupa a linha de fogo. Sua arma usa 12 projéteis por ciclo.
Depois do primeiro disparo, o intervalo cai gradualmente de 0,70 s até
0,16 s entre os últimos tiros. O 12º inicia uma recarga de 2,4 s; nesse
período ele não dispara. A munição volta ao fim da recarga para que uma luta
longa não deixe o Boss inerte. A IA roteirizada e a neural usam o mesmo
carregador e a mesma cadência. Perder o alvo ou segurar fogo por um aliado não
gasta balas nem adianta a sequência. A IA neural não pode pedir recarga cedo.

No Normal, cada projétil de longa distância causa 8,5 HP, comparado com 18 HP
do antigo disparo espaçado. Pressão e perseguição preservam suas proporções de
dano; dificuldade continua escalando os parâmetros de fase. Em exposição
contínua, a média teórica de um ciclo completo fica próxima da antiga
(aproximadamente 14 HP/s), mas a parte final da rajada é mais perigosa. O
usuário aprovou a rajada após jogar; manter esses valores como base desta
versão.

Os projéteis do Boss danificam crates, barricadas, canisters e luminárias que
interceptarem sua trajetória. Paredes e portas fechadas param o tiro antes do
objeto; props protegidos, como o terminal, absorvem o impacto. A destruição
pode deixar o drop normal do objeto, uma única vez. O tiro do inimigo não
concede XP de destruição ao jogador. Outros inimigos continuam tendo seus
tiros bloqueados pela cobertura sem danificá-la. Tiros e recarga do Boss
produzem áudio posicional; a arma visível continua usando o socket já alinhado
à mão direita.

Validação automatizada: combat testa tempos de 13 disparos, pausa, recarga,
perda de alvo e controle roteirizado/neural; enemy_sensors testa cobertura,
aliado, parede, drop único, terminal e salas adjacentes. A aplicação e a
renderização na NVIDIA são gates adicionais. Nada nesta etapa promove um
novo `assets/ai/*.brain`; avaliações de IA anteriores precisam ser refeitas
na nova mecânica antes de uma promoção.

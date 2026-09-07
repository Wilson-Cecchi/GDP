# Enemies — Animação e IA

> **Estado:** ciclo mínimo Idle/Chase/Attack/Dead do Worker validado na Fase 7 e
> Guard, Officer, Engineer e IndustrialRobot validados na Fase 11. A Fase 86
> adicionou desvio local determinístico de obstáculos aos cinco arquétipos;
> animação, patrulha, investigação e pathfinding global permanecem planejados.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Animação

Começar com hierarquia rígida por partes: root → torso → cabeça/braços/pernas. Estados
mínimos: Idle, Walk, Attack, Hit e Death. Não construir um sistema universal de animação.

## Walk

Pode ser paramétrico por fase; sockets preservam conexão entre partes. Peças mecânicas podem
usar amplitudes próprias.

## IA base

Estados: Idle, Patrol, Investigate, Alert, Chase, Attack, Recover, Dead.
Percepção usa distância, line of sight, ruído recente e estado da sala.

O Worker atual detecta em até `12 m`, persegue a `1,35 m/s`, ataca dentro de
`1,15 m` com line of sight e causa 10 de dano a cada `1 s`. Movimento reutiliza
o collider da sala; estado Dead bloqueia movimento e ataque definitivamente.

O Guard detecta em até `16 m`, avança a `1,10 m/s` até seu envelope de `4–8 m`,
recua abaixo de `4 m` e dispara 8 de dano a cada `0,80 s`. Sem line of sight ele
entra em Chase; dentro do envelope só entra em Attack quando o ray até a câmera
não encontra um sólido. Dead bloqueia também Reposition.

O Officer detecta em até `18 m`, avança a `0,95 m/s` para seu envelope de
`6–10 m`, recua abaixo de `6 m` e dispara 6 de dano a cada `1,20 s`, sempre com
line of sight. A cada `2,50 s`, pode reduzir em `0,35 s` o cooldown de ataque de
Workers e Guards vivos dentro de `10 m` e no mesmo `roomId`; ataque e comando
possuem cooldowns independentes.

O Engineer detecta em até `14 m`, opera a `1,05 m/s` no envelope de `3–6 m` e
causa 5 de dano a cada `1 s`, sempre com line of sight. A cada `2 s`, escolhe o
alvo reparável visível mais próximo em até `7 m`: recupera 12 HP de um Enemy
vivo ou muda um Prop de `Damaged` para `Intact`. Nunca repara mortos,
`Destroyed`, alvos de outra sala ou atrás de sólidos.

## Roles

Worker aproxima e ataca; Guard procura linha de tiro; Officer altera ameaça do grupo;
Engineer opera sistemas; IndustrialRobot muda de fase conforme dano.

## Boss

IndustrialRobot detecta em até `24 m`. Acima de 66% de HP opera a `0,75 m/s`
em 8–13 m e causa 12 de dano a cada `1,40 s`; entre 33–66% opera a `1 m/s` em
4–8 m e causa 10 a cada `0,90 s`; em 33% ou menos persegue a `1,35 m/s` até
2,25 m e causa 16 a cada `0,65 s`. As fases remotas recuam abaixo do alcance
mínimo. Todo ataque exige line of sight e Dead encerra movimento e transições.

## Navegação

A* sobre grid/grafo local é suficiente inicialmente. Não criar navmesh universal antes de necessidade.

Antes de um pathfinding global, a Fase 86 implementa o caso frequente de
obstáculo local. O movimento tenta a direção desejada; quando a colisão deixa
menos de 45% de progresso frontal, escolhe esquerda ou direita por
`visualSeed + EntityId` e mantém o desvio por `1,6 s`. Um lado sem deslocamento
troca para o oposto. A movimentação continua passando por `CollisionWorld`, de
modo que caixas, paredes, limites e portas continuam autoritativos.

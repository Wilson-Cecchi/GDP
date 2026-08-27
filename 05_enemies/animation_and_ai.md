# Enemies — Animação e IA

> **Estado:** ciclo mínimo Idle/Chase/Attack/Dead do Worker validado na Fase 7 e
> Guard e Officer com Reposition validados na Fase 11; animação, patrulha, investigação e
> navegação avançada permanecem planejadas.
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

## Roles

Worker aproxima e ataca; Guard procura linha de tiro; Officer altera ameaça do grupo;
Engineer opera sistemas; IndustrialRobot muda de fase conforme dano.

## Boss

Base inicial: >66% longa distância, 33–66% maior pressão, <33% perseguição/curto alcance.
Percentuais finais permanecem balanceáveis.

## Navegação

A* sobre grid/grafo local é suficiente inicialmente. Não criar navmesh universal antes de necessidade.

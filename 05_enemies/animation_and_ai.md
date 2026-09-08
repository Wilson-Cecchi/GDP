# Enemies — Animação e IA

> **Estado:** ciclo mínimo Idle/Chase/Attack/Dead do Worker validado na Fase 7 e
> Guard, Officer, Engineer e IndustrialRobot validados na Fase 11. A Fase 86
> adicionou desvio local determinístico de obstáculos aos cinco arquétipos;
> a Fase 87 adicionou separação local entre aliados e a Fase 88 consolidou
> volumes físicos entre inimigos. A Fase 90 implementou poses procedurais de
> raiz para Idle, Walk, Attack e Death, e a Fase 91 adicionou orientação pelo
> deslocamento/alvo. A Fase 92 suavizou o yaw por fixed update; hierarquia por
> membros, Hit, patrulha, investigação e pathfinding global permanecem
> planejados.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Animação

A Fase 90 introduziu a primeira camada funcional: a `Application` converte o
comportamento atual em Idle, Walk, Attack ou Dead, e o renderer amostra uma pose
de raiz determinística. Idle usa respiração contida, Walk possui elevação e
compressão mais legíveis, Attack alonga a silhueta e Death preserva a
pose achatada terminal já existente. Os ciclos avançam nos fixed updates,
congelam em pausa/resultado e recebem apenas uma defasagem da `visualSeed`, sem
consumir RNG nem alterar gameplay. Inimigos pré-carregados ainda dormentes ficam
estáticos.

A Fase 91 completa a orientação da raiz. Chase usa o deslocamento efetivo após
colisão, separação e desvio; Attack encara o jogador; os arquétipos remotos
recuam em Reposition ainda voltados para o alvo. Idle, Death e deslocamentos
nulos conservam o último yaw válido. O eixo frontal dos assets é `+Z`, coerente
com os sockets traseiros em `-Z`, e o snapshot visual transporta a rotação já
decidida pelo gameplay.

A Fase 92 limita a aproximação angular pelo menor arco: `480°/s` ao acompanhar
o deslocamento de Chase e `720°/s` ao mirar em Attack/Reposition. O cálculo usa
o mesmo fixed step da IA e não depende de FPS. Reposition não acumula dois giros
no mesmo tick: o deslocamento de recuo não consome rotação, apenas a direção do
jogador. Idle e Death continuam congelando o último yaw.

Os assets procedurais atuais são meshes consolidadas. Portanto esta etapa move e
escala o inimigo inteiro; ela não finge oferecer articulação que o formato ainda
não suporta. A próxima evolução visual deve separar uma hierarquia rígida por
partes — root → torso → cabeça/braços/pernas —, preservar sockets e então
adicionar Hit e amplitudes próprias por arquétipo. Não construir um sistema
universal de animação.

## Walk

A pose de raiz já é paramétrica por fase. Quando os meshes forem separados,
sockets devem preservar a conexão entre partes e peças mecânicas podem usar
amplitudes próprias.

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

A Fase 87 acrescentou repulsão suave antes desse desvio. Somente aliados vivos e
ativos no mesmo `roomId` entram no cálculo; a distância preferida combina os
raios físicos e uma margem de `0,20 m`. Sobreposição exata escolhe sentidos
opostos pela ordem estável dos `EntityId`. Isoladamente, essa resposta não era
um collider e podia permitir cruzamentos em grupos densos.

A Fase 88 mantém o steering para fluidez, mas acrescenta contato físico no
gameplay. O segmento completo de cada deslocamento é testado contra os raios dos
outros inimigos, evitando penetração e troca de lados mesmo em um passo grande.
Sobreposições preexistentes só podem aumentar a distância, e estados Idle ou
Attack aplicam uma correção lenta. Paredes, portas e props continuam resolvidos
por `CollisionWorld`, preservando passagem e autoridade da geometria.

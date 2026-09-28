# Enemies — Animação e IA

> **Estado:** ciclo mínimo Idle/Chase/Attack/Dead do Worker validado na Fase 7 e
> Guard, Officer, Engineer e IndustrialRobot validados na Fase 11. A Fase 86
> adicionou desvio local determinístico de obstáculos aos cinco arquétipos;
> a Fase 87 adicionou separação local entre aliados e a Fase 88 consolidou
> volumes físicos entre inimigos. A Fase 90 implementou poses procedurais de
> raiz para Idle, Walk, Attack e Death, e a Fase 91 adicionou orientação pelo
> deslocamento/alvo. A Fase 92 suavizou o yaw por fixed update e a Fase 93
> adicionou uma reação Hit de raiz. A Fase 94 separou a hierarquia rígida e
> articulou os membros; a Fase 95 especializou o movimento por arquétipo e a
> Fase 96 suavizou as transições e removeu a morte achatada. A Fase 97 evoluiu
> essa queda para um ragdoll procedural articulado; a Fase 98 separou perda de
> sustentação, inércia, queda e contato para remover a rigidez remanescente.
> A Fase 99 abriu a direção da queda para 360° e tornou o repouso da última
> parte uma condição explícita do fim da transição visual.
> A Fase 100 implementou patrulha local determinística nos cinco arquétipos.
> A Fase 101 implementou aquisição visual e investigação da última posição
> conhecida. A Fase 103 acrescentou percepção por ruído de disparo e a Fase
> 104 implementou a reação dedicada `Alert`. A Fase 105 acrescentou `Recover`
> ao fim da busca, a Fase 106 implementou pathfinding global dentro da sala e a
> Fase 107 limitou aquisição ao cone de visão de cada papel.
> A Fase 108 distribuiu grupos por rotas equivalentes e replanejamento lateral.
> A Fase 102 acrescentou na START uma cobertura larga para validar visualmente
> aquisição, perda de contato, investigação e readquisição no primeiro combate.
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

A Fase 93 adiciona o estado visual Hit sem interromper a máquina de IA. Cada
acerto não letal reinicia uma janela de `0,12 s`, avançada uma única vez no
fixed update; durante ela, a raiz recebe compressão, tonalidade quente e
emissivo reforçado. Death possui prioridade e limpa o timer. Como movimento,
ataque e cooldowns continuam normalmente, o feedback melhora a leitura do
acerto sem criar hit stun ou alterar o balanceamento.

A Fase 94 substitui a transformação consolidada por uma hierarquia rígida de
partes. As 8 primitivas de Worker/Guard/Officer/Engineer e as 9 do
IndustrialRobot recebem meshes GPU independentes e pivôs estáveis em raiz,
pescoço, ombros e quadris. Walk movimenta braços e pernas em oposição com
amplitudes de `8°/10°`; Attack recua braços e a ferramenta; Idle conserva só
`0,8°` de leitura da cabeça. A ordem da receita define a função de cada parte e
o split preserva integralmente vértices e índices da geometria consolidada.

O Hit também foi revisto para a direção séria do jogo. A raiz mantém proporções
e altura; torso, cabeça, braços e ferramenta executam um recuo rígido pequeno,
escalado pela fração restante do timer. Tint e emissivo retornam gradualmente ao
material normal. Não há squash and stretch, deslocamento de collider ou hit
stun.

A Fase 95 troca o perfil comum por cinco assinaturas determinísticas. Worker
mantém a maior passada e avança a ferramenta em Attack, coerente com combate
corpo a corpo. Guard reduz balanço e recua a arma rapidamente; Officer usa a
postura mais contida. Engineer aumenta o movimento do braço direito para tornar
a ferramenta legível. IndustrialRobot usa o ciclo Walk mais longo (`48 ticks`),
amplitudes de `3°/5°`, recuo pesado da arma e apenas `1,3°` de resposta do torso
ao Hit. Os outros ciclos também usam períodos próprios, evitando que papéis
diferentes pareçam compartilhar a mesma atuação.

Os assets procedurais continuam definidos por receitas consolidadas, mas o
upload preserva cada primitiva como parte rígida articulável. Os perfis ficam no
conteúdo visual concreto e não formam um sistema universal de animação.

A Fase 96 acrescenta um histórico visual mínimo por entidade. A primeira pose
observada começa pronta; mudanças seguintes interpolam a pose anterior e a
atual por fixed ticks e `smoothstep`, com resposta mais rápida para Hit e mais
longa para Death. A morte tem prioridade, preserva todas as proporções e inclina a raiz
rigidamente em `82°` para um lado determinado pela paridade da `visualSeed`.
Esse assentamento continua atrás do resultado, mas nenhum estado vivo, IA ou
combate volta a avançar. Reiniciar a run descarta integralmente o histórico.

Na Fase 97, a raiz deixou de carregar sozinha toda a leitura da morte. Cada
parte usa seu pivô anatômico já validado e recebe uma pose terminal derivada do
papel e da `visualSeed`. A progressão local introduz atrasos de até 24% da janela
e uma oscilação que converge a zero, simulando transferência e perda de energia
sem consumir RNG. O Worker tem articulação de referência; Guard e Officer são
mais contidos; Engineer é ligeiramente mais solto; IndustrialRobot usa menos da
metade da amplitude e maior compensação vertical de contato com o piso.

Este é deliberadamente um ragdoll visual híbrido, não um solver físico da WKC
Forge. Cadáveres não ganham colliders por membro, não colidem com props/paredes
e não alteram navegação, hitscan, vitória ou balanceamento.

A Fase 98 retifica o primeiro timing de Death após validação dentro do jogo. O
progresso terminal deixa de receber o `smoothstep` único das demais transições:
as pernas começam em 0–3%, a raiz em 10%, braços entre 14–22%, cabeça em 18% e
equipamentos por volta de 24%, com pequena variação estável por seed. A queda do
torso termina sua fase principal em 78%; os 22% finais absorvem o contato no
piso. Humanos usam 36–40 ticks e o IndustrialRobot usa 52 ticks, reforçando a
diferença de massa sem alterar o instante lógico da morte.

Na Fase 99, Death deixa de escolher apenas um dos dois lados. Um azimute
contínuo, obtido diretamente da `visualSeed`, orienta a inclinação de `88°` e o
impulso da raiz no plano XZ; assim, frente, trás, laterais e diagonais aparecem
sem estado aleatório mutável. O contato também passa a ser escalonado até o fim:
pernas, torso, acessórios, cabeça, braços e ferramenta possuem pares próprios
de primeiro contato/repouso, e a última peça só zera sua oscilação em 100%.
Worker/Engineer usam 48 ticks, Guard 50, Officer 52 e IndustrialRobot 66. Todas
as partes coincidem exatamente com suas poses terminais quando a transição
encerra; a morte lógica continua imediata.

A Fase 104 acrescenta uma pose `Alert` rigidamente articulada. A raiz inclina
`2°` e avança `0,012 m` no Worker; torso, cabeça, braços e equipamento assumem
uma prontidão curta, escalada por papel até `48%` no IndustrialRobot. A entrada
usa blend de 3 ticks e a saída, 4, sem alterar colliders ou produzir deformação.
Hit continua prevalecendo visualmente e Death permanece terminal.

A Fase 105 acrescenta uma pose `Recover` de proporções rígidas. A cabeça varre
até `14°` para cada lado enquanto torso e braços preservam prontidão residual;
o perfil de massa reduz a amplitude até `48%` no IndustrialRobot. Entrada e
saída usam blends de 5 ticks. A raiz conserva escala `1:1:1`, e nenhuma parte
visual altera orientação lógica, collider ou alcance de combate.

## Walk

A pose de raiz continua paramétrica por fase e os meshes separados giram ao
redor dos sockets preservados. Cada arquétipo escolhe período e amplitude sem
alterar o deslocamento lógico da entidade.

## IA base

Estados implementados: Idle, Patrol, Alert, Investigate, Recover, Chase,
Reposition, Attack e Dead. A percepção combina visão direta e estímulo de ruído
de disparo na sala ativa; a navegação global permanece confinada à sala atual.
Aquisição exige alcance e line of sight dentro da sala ativa.

O Worker atual detecta em até `12 m`, persegue a `1,35 m/s`, ataca dentro de
`1,15 m` com line of sight e causa 10 de dano a cada `1 s`. Movimento reutiliza
o collider da sala; estado Dead bloqueia movimento e ataque definitivamente.

O Guard detecta em até `16 m`, avança a `1,10 m/s` até seu envelope de `4–8 m`,
recua abaixo de `4 m` e dispara 8 de dano a cada `0,80 s`. Sem line of sight ele
só investiga uma posição adquirida anteriormente; dentro do envelope só entra
em Attack quando o ray até a câmera não encontra um sólido. Dead bloqueia
também Reposition.

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

## Treino neural experimental por arquétipo

O currículo 2 mantém a topologia e o formato dos cérebros existentes, mas deixa
de avaliar todos os papéis nas mesmas quatro arenas. Cada arquétipo tem quatro
situações de treino e duas situações separadas de validação, com duas variações
de seed por situação. Os padrões variam cobertura, orientação inicial,
movimento do Player e frequência de disparos recebidos. Worker é avaliado por
aproximação e contato corpo a corpo; Guard por faixa de tiro e alinhamento;
Officer por tiro e posição útil ao comando; Engineer por sobrevivência e posição
útil ao reparo. O IndustrialRobot começa em diferentes faixas de HP, incluindo
as três fases, e executa episódios de 12 s em vez de 8 s. Seu intervalo de
distância desejado acompanha a fase atual.

As ações neurais de ataque remoto só acertam após alinhamento de 12° com o
Player, enquanto o Worker mantém uma tolerância de 35° para combate próximo.
Quando a visão se perde, o inimigo selecionado volta à investigação e à
navegação normais com a última posição conhecida; a rede não recebe a posição
real de um jogador oculto nem continua repetindo saídas de movimento obsoletas.
Mira alinhada e suporte entram na pontuação com limites para não dominar dano
e acertos. Comando e reparo permanecem automáticos no gameplay; o treino pode
melhorar o posicionamento que os viabiliza, mas não atribui à rede a decisão de
executá-los. Uma política inicial de percepção, giro e ataque dá ao processo um
ponto de partida; descendentes e indivíduos aleatórios preservam exploração.
`curriculum=2` nos logs distingue os resultados: pontuações de treinos
anteriores não são comparáveis, embora seus arquivos `.brain` continuem
carregáveis para jogo ou `--resume`. Um treino novo, sem `--resume`, é preferível
para aproveitar a política inicial e o alinhamento de mira mais restrito. A
integração no jogo segue opt-in para um
único inimigo por vez, não substitui a IA normal dos demais.

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

A Fase 100 adiciona a primeira navegação fora de combate. Cada Enemy guarda o
ponto de spawn como origem e percorre waypoints de raio curto derivados de
`EntityId`, `roomId` e índice por hash estável. O movimento usa velocidade
reduzida por papel, gira pela mesma curva de Walk e compartilha integralmente
`CollisionWorld`, desvio local e volumes entre inimigos. Ao alcançar o destino,
o inimigo pausa antes do próximo; se o destino permanecer bloqueado além do
timeout, a rota avança em vez de insistir contra a parede. Detecção assume
prioridade no mesmo tick. Player morto, Dead, entidades dormentes e outras
salas não avançam patrulha. Nenhum stream de RNG ou pathfinding global foi
introduzido.

A Fase 101 separa posição real e conhecimento do inimigo. Uma observação válida
armazena a posição do Player; ao perder linha de visão ou alcance, o inimigo
segue somente esse último ponto durante `3,5 s` (Worker), `4,5 s` (Guard),
`5 s` (Officer), `4 s` (Engineer) ou `6 s` (IndustrialRobot), em velocidade
reduzida por papel. A memória não acompanha o Player oculto. Chegada antecipa
Idle, mas mantém a janela de observação; expiração retorna ao ciclo local de
patrulha e uma nova visão substitui a memória no mesmo tick. Player morto limpa
o timer, e a morte do inimigo limpa posição e timer. O processo não consome RNG
nem introduz pathfinding global.

A Fase 103 permite iniciar a mesma investigação sem aquisição visual. Disparos
consumidos geram um estímulo de raio-base `18 m`; o alcance efetivo é o menor
entre esse raio e a audição do papel: `10 m` Worker, `15 m` Guard, `17 m`
Officer, `13 m` Engineer e `22 m` IndustrialRobot. O sinal atravessa cobertura,
mas não atravessa o limite lógico da sala ativa e ignora inimigos dormentes ou
mortos. Cooldown e clique seco não produzem estímulo. Uma visão confirmada no
fixed update sempre substitui a origem ouvida pela posição realmente observada.
O processo não consome RNG e permanece independente da reprodução de áudio.

A Fase 104 separa a reação inicial em `Alert`. Ao primeiro ruído sem memória
ativa, o inimigo fica imóvel, vira para a origem e suspende ataque, suporte e
reparo por `0,18–0,34 s`, conforme o papel. A investigação começa depois dessa
janela. Um novo ruído atualiza a posição e renova o tempo de busca, mas não
reinicia `alertTime`; assim, cadência de tiros não paralisa o elenco. Visão
confirmada atualiza a posição durante Alert e assume o combate quando a reação
termina. Player morto e morte do inimigo limpam o estado.

A Fase 105 insere `Recover` quando o timer de investigação expira. Cada papel
permanece entre `0,65 s` e `1,20 s` em uma varredura estacionária: o yaw gira a
`70°/s` para um lado derivado de `visualSeed + EntityId` e inverte na metade da
janela. Ao final, a posição lembrada é apagada e a patrulha retorna. Linha de
visão cancela a recuperação com prioridade imediata; ruído novo retoma a busca
sem repetir `Alert`. Player morto e morte do inimigo limpam o timer.

A Fase 106 substitui a dependência exclusiva do desvio reativo por um grafo de
visibilidade reconstruído deterministicamente quando o caminho direto está
bloqueado. Origem, destino e cantos com folga física formam os nós; somente
segmentos inteiramente ocupáveis viram arestas. O menor caminho orienta Chase,
Investigate e Patrol, enquanto steering, separação e colisão resolvem o
movimento final. Rotas inexistentes mantêm o fallback local e seus timeouts.

A Fase 107 combina orientação e percepção: o yaw lógico define o vetor frontal
usado por uma meia abertura de `65°/75°/80°/70°/85°` para
Worker/Guard/Officer/Engineer/IndustrialRobot. Alcance e line of sight continuam
obrigatórios. Patrulha, Chase, Investigate, Alert e Recover já giram o corpo e,
portanto, varrem o cone sem estado visual paralelo. Ruído pode chegar por trás e
usa Alert para orientar o inimigo antes da confirmação visual.

Na Fase 153, a lanterna ligada amplia apenas o alcance de aquisição visual do
Player em 25%, com teto de 4 m adicionais. A regra vale para os cinco papéis,
sem ignorar o cone frontal ou a linha de visão: uma parede ou o lado de trás
continuam impedindo a detecção visual. Lanterna desligada preserva os alcances
anteriores, e esse sinal transitório não entra em saves nem consome RNG.

A Fase 108 acrescenta diversidade determinística ao mesmo grafo. A paridade da
identidade define qual lado de um sólido entra primeiro no desempate; quando a
colisão ativa `avoidanceSide`, o próximo cálculo usa esse lado e consegue
replanejar. Separação e teste contínuo dos volumes permanecem no movimento
final, portanto a escolha de rota não permite atravessar aliados.

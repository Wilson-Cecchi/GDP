# Revisão do treinamento e das recompensas da IA — 2026-09-29

**Estado:** auditoria da implementação atual e proposta de migração. Este documento
não altera o combate, os cérebros distribuídos em `assets/ai/` nem o formato dos
checkpoints. Os valores de recompensa abaixo são critérios de desenho, não um
balanceamento aprovado. O pedido detalhado está no texto fornecido pelo usuário
em 2026-09-29; esta revisão se baseia também no código e nos testes atuais.

## 1. Mapa da implementação existente

| Área solicitada | Estado verificado | Código principal |
| --- | --- | --- |
| Movimento do Player | `PlayerCommand` oferece frente/lado, agachar, caminhar silenciosamente e pular. `Player::fixedUpdate` limita cada eixo a `[-1, 1]`, soma bases ortogonais, normaliza somente quando o módulo supera 1 e aplica velocidade de postura. A diagonal já não ganha velocidade. Bhop permite conservar e ganhar impulso limitado por air-strafe e saltos encadeados. | `src/gameplay/Player.hpp`, `Player.cpp`, `tests/test_gameplay.cpp` |
| Movimento dos inimigos | A IA roteirizada escolhe destino e orientação via navegação; a neural emite movimento local. `moveEnemy` aplica separação, colisão e desvio. O vetor neural tem velocidade limitada, mas o lateral é multiplicado pela intenção frontal e limitado a 25%; um comando só lateral vira um passo frontal. Isso **não** representa dois eixos independentes. Avanço negativo ainda produz recuo real. | `src/gameplay/Enemy.cpp` |
| Representação de ações | Cada cérebro tem 7 entradas, 8 neurônios ocultos e 4 saídas: frente, lado, giro, pedido de ataque. As saídas são atualizadas a 10 Hz. Não há ação neural de recarga, escolha de cobertura, salto, suporte ou arma. A política não tem memória recorrente; informação temporal só existe nos estados roteirizados externos. | `src/gameplay/EnemySensors.hpp`, `Enemy.cpp`, `src/ai/NeuralNetwork.cpp` |
| Direção e normalização | O Player usa yaw 0° em `+X`; o Enemy usa yaw 0° em `+Z`. São convenções locais diferentes, mas ambas têm bases ortogonais frente/lado. Player normaliza a soma acima de 1; Enemy calcula direção normalizada e velocidade `min(|vetor|,1) * moveSpeed`, portanto não excede o limite físico. A restrição neural lateral é uma **política**, não uma necessidade da normalização. | `src/gameplay/Player.cpp`, `Enemy.cpp` |
| Recompensa | `scoreEnemyFitness` usa dano, acertos, aproximação, tempo na faixa, contato, mira, suporte, sobrevivência, dano recebido, inatividade, bloqueio, ataque inútil e morte. Não há pontuação explícita por eliminar o Player, direção apropriada de recuo, oscilação, tiro errado, exposição ou esquiva. Termos contínuos têm alguns limites, mas dano bruto pode dominar a pontuação. | `src/game/ai/WorkerTraining.cpp` |
| Treino | Cinco currículos por arquétipo, cada um com 6 configurações de trajetória/cobertura. Currículo padrão: 4 de treino e 2 de holdout, 2 repetições; `--all-scenarios`: 6 no treino e os mesmos tipos de situação com outras seeds no holdout. Arena plana de 32 × 32 m, episódios de 8 s (12 s no boss). Não há estágios de locomoção, mira e combate separados. | `src/game/ai/WorkerTraining.cpp` |
| Tipos e papéis | Existem exatamente Worker (corpo a corpo), Guard (atirador), Officer (tiro e comando), Engineer (tiro e reparo), IndustrialRobot (boss de três fases). Os perfis já diferenciam alcance, velocidade e dano; não existem arquétipos reais `Shotgun`, `SMG` ou `Sniper`. | `src/entities/EntityManager.hpp`, `src/gameplay/Enemy.hpp` |
| Armas dos inimigos | Os inimigos não instanciam `Weapon`. Seus ataques usam dano/cadência/alcance de `*Config`, com dano direto ao Player após teste de visão, alinhamento e cooldown. Comando/reparo são regras separadas. As armas `Pistol`, `Carbine` e `Knife` pertencem ao sistema do Player. | `src/gameplay/Enemy.cpp`, `Weapon.hpp` |
| Tiros/projéteis | O Player atira por hitscan: há consumo de munição, spread determinístico, interseção com inimigos/props e obstrução pelo cenário. Não há projétil inimigo com trajetória/tempo de voo, evento de tiro inimigo identificável, munição inimiga ou recarga inimiga. No treino, o Player de teste inflige dano sintético em intervalos quando há linha de visão. | `src/gameplay/Combat.cpp`, `WorkerTraining.cpp` |
| Linha de visão | Alcance, cone frontal e raycast contra sólidos/volumes limitam percepção e ataque. Sem visão atual, a rede cede o controle à investigação/navegação roteirizada; a posição oculta do Player não entra nos sensores. | `src/gameplay/Enemy.cpp`, `EnemySensors.cpp`, `Combat.cpp` |
| Colisões | `CollisionWorld` testa volumes transitáveis e sólidos, resolve movimento por eixos e informa colisão. `moveEnemy` também separa inimigos e desvia. O sensor de paredes usa collider fixo de raio 0,35 m/altura 1,8 m, embora o boss use raio 0,52 m/altura 1,9 m no movimento: a percepção de folga pode discordar da colisão do boss. | `src/gameplay/Collision.cpp`, `Enemy.cpp`, `EnemySensors.cpp` |
| Navegação | Grafo de visibilidade, waypoints, steering/separação e desvio reativo já resolvem parte dos obstáculos no jogo. O treino usa só um volume de arena e até uma cobertura por cenário; não ensina portas, corredores ou conexões de salas. | `src/gameplay/Enemy.cpp`, `WorkerTraining.cpp` |
| Combate | A rede pede ataque; alcance, mira (35° Worker/12° demais), visão e cooldown autorizam dano. No jogo normal (`controlAll=true`), assistências de mira e retorno temporário à IA roteirizada evitam inimigos inofensivos. O treino roda com `controlAll=false`; portanto não reproduz exatamente a política de produção. | `src/gameplay/Enemy.cpp`, `src/game/Application.cpp`, `WorkerTraining.cpp` |

## 2. Lacunas que afetam diretamente o aprendizado

1. **Crédito misturado:** no treino, dano, acertos e suporte são acumulados
   mesmo em ticks nos quais a rede não controlou o inimigo. Sem visão, a IA
   roteirizada assume. O Engineer e o Officer também recebem pontos por ações
   automáticas de reparo/comando que a rede não escolheu. Separar `neural`,
   `scripted_fallback` e `automatic_support` é pré-requisito para interpretar o
   fitness. A documentação atual diz que suporte não entra no fitness, mas o
   código pontua `supportActions`: corrigir a divergência junto com a migração.
2. **Modelo de treino diferente do jogo:** treino usa um inimigo experimental
   com `controlAll=false`; produção usa um estado de decisão por entidade,
   assistências e fallback de 1,5 s após 2 s sem progresso. O avaliador de
   gameplay exercita `controlAll=true`, mas atualmente mede apenas poucos
   resultados agregados; seu teste automatizado verifica reprodutibilidade,
   não superioridade tática.
3. **Aproximação não é progresso sustentável:** `approachMeters` mede a melhora
   até o melhor instante do episódio. É possível se aproximar uma vez e depois
   recuar/oscilar sem devolver esse crédito. Faixa ideal, mira e contato são
   parcialmente limitados, mas não há detecção de alternância inútil. Não
   premiar simplesmente a saída `forward > 0`.
4. **Ações planejadas não existem:** tiro errado, rajada, munição, recarga e
   esquiva de projétil não podem ser medidos honestamente com o ataque inimigo
   atual. Em hitscan, esquiva após o disparo é impossível; o comportamento
   treinável é antecipar a ameaça de mira/disparo ou entrar em cobertura.
5. **Holdout estreito:** especialmente em `--all-scenarios`, o holdout troca
   seeds, mas repete a mesma família de arena e trajetória. Não comprova
   generalização para portas, corredores, altura ou jogadores reais.
6. **Observação incompleta:** faltam sinais para ameaça de tiro, velocidade do
   Player, cobertura, aliados, munição e estado de arma. Aumentar sensores ou
   mudar semântica das saídas requer versionar o contrato `.brain` e treinar
   modelos novos; não reinterpretar silenciosamente os arquivos atuais.
7. **Avaliação de combate incompleta:** `gdp_ai_gameplay_eval` move/reloca o
   Player, mas não o faz atirar. Nesse teste os inimigos não morrem; portanto
   `defeat_frame` mede apenas se o Player foi eliminado, não uma taxa de vitória
   em duelo justo. O treino, ao contrário, aplica dano sintético periódico aos
   inimigos. Esse descompasso precisa ser fechado antes de usar win rate como
   gate de promoção.

## 3. Contratos propostos

### Movimento físico e atribuição de crédito

Para Player e Enemy, compor em bases locais ortonormais:

```text
f = clamp(forward, -1, 1)
s = clamp(strafe, -1, 1)
raw = forward_basis * f + right_basis * s
movement = raw / max(1, length(raw))
velocity = movement * role_or_stance_speed
```

`(1, 1)` resulta em `(0,7071, 0,7071)` no espaço local e `|movement| = 1`.
Não transformar diagonal em `(0,5, 0,5)`. No Enemy, remover a vinculação
forçada entre lateral e frente **apenas para cérebros de contrato novo**, pois
os pesos atuais foram treinados com outra semântica.

O crédito diagnóstico é separado da física: se `|f| + |s| > 0`, as frações
`|f|/(|f|+|s|)` e `|s|/(|f|+|s|)` somam 1; os sinais preservam se a ação foi
avanço/recuo e esquerda/direita. Com `(1,1)`, cada eixo recebe 50% da
atribuição, mas o vetor físico continua normalizado para comprimento 1.
Essas frações **não são recompensa automática**: só atribuem a um comando o
resultado tático realmente observado. Quando dois comandos cooperam, essa
fração é uma aproximação; não substitui testes de resultado/ablação.

### Recompensas

- Pontuar por **episódio e por entidade**, com autoria (`neural`, `fallback`,
  `automatic`) explícita. Eventos terminais de vitória/derrota são dominantes;
  dano causado/recebido usa escala por HP, não valores brutos sem teto.
  Como hipótese inicial de calibração, vitória `+10` e morte `-10`, com a soma
  máxima de todos os termos auxiliares contínuos por episódio abaixo de `10`.
  Pesos definitivos dependem da frequência medida de cada evento.
- Para a faixa tática `[min, max]`, definir erro 0 dentro dela, `min-d` abaixo
  e `d-max` acima. Usar progresso potencial `erro_antes - erro_depois`,
  limitado por episódio; não pontuar a tecla/saída frente por si só. Isso
  permite recuar quando muito perto, avançar quando longe e usar diagonal.
- Usar `delta time` para taxas de exposição/posição. Cada termo contínuo tem
  teto por episódio; aquisição/perda repetida de cobertura ou visão não gera
  bônus repetido. Suporte só entra no fitness quando for escolha da política.
- Oscilação exige reversão de comando **sem** melhora de potencial, mira ou
  redução de risco. Reação a ameaça real não recebe penalidade. Colisão é
  contabilizada a partir do resultado físico, distinguindo parede de ally.
- Adiar recompensa por tiro desperdiçado, recarga e esquiva até existir evento
  de disparo/ameaça verificável. Não fabricar uma métrica a partir de um simples
  pedido de ataque ou de um timer de dano sintético.

### Métricas e avaliação

Registrar por arquétipo e por cenário: episódios, vitórias/derrotas/timeouts,
dano causado/recebido, duração e tempo até eliminação, distância média e erro
da faixa ideal, tempo exposto/em cobertura, colisões, tempo preso, segundos de
avanço/recuo/strafe (inclusive diagonais), reversões sem progresso, ataques
solicitados/autorizados, acertos, tempo de aquisição/tracking e proporção de
ticks neurais versus fallback. Quando existir evento de disparo real, acrescentar
tiros disparados/acertados, precisão, desperdício e recargas. Quando existir
ameaça observável verificável, acrescentar oportunidades de esquiva e esquivas
bem-sucedidas; até lá, apresentar `N/A` em vez de inventar zero. Para calcular
win rate, o avaliador deve permitir que o Player ataque e elimine inimigos.

Comparar versões na **mesma** bateria fixa de seeds e geometrias não usadas no
treino, depois em seeds aleatórias independentes; publicar também intervalos ou
distribuição, não só média/reward. Um candidato precisa melhorar resultados
relevantes sem regredir criticamente navegação, cobertura ou comportamento
visual, e precisa de playtest antes de entrar em `assets/ai/`.

### Perfis reais

| Arquétipo | Papel e faixa atual | Objetivo de treino inicial |
| --- | --- | --- |
| Worker | Melee, 0–2 m no treino | Aproximar/contornar, não orbitar sem ataque, recuo apenas por ameaça real. |
| Guard | Tiro direto, 4–8 m | Alinhar e manter linha de visão/faixa, reposicionar quando obstruído. |
| Officer | Tiro direto + comando automático, 6–10 m | Mira e posição com visão/alcance dos aliados; não creditar comando automático à rede. |
| Engineer | Tiro direto + reparo automático, 3–6 m | Sobrevivência e posição útil ao reparo, sem farm de ação automática. |
| IndustrialRobot | Três fases: 8–13, 4–8 e 0–2,25 m | Distância por fase, navegação por cobertura/altura e pressão real sem marcha ré permanente. |

Esses intervalos são os perfis de treino existentes, não categorias de armas
introduzidas por esta proposta. Se munição e armas forem adicionadas aos
inimigos, o perfil poderá incorporar arma/cadência/cobertura sem inventar
arquétipos `Shotgun`, `SMG` ou `Sniper`.

## 4. Sequência de implementação proposta

1. **Baseline e telemetria (sem troca de cérebro):** congelar seeds/versões;
   adicionar eventos por inimigo para decisão, autoria, movimento solicitado e
   efetivo, colisão, visão, cobertura, dano, ataque autorizado, fallback e
   morte. Ampliar `gdp_ai_gameplay_eval` com duração, vitória/derrota, dano,
   faixa média, tempo exposto/em cobertura, colisões, tempo preso, sentidos de
   movimento, reversões e proporção de ticks neurais. Reportar `N/A` para
   precisão de tiro, recarga e esquiva enquanto não existirem eventos reais.
2. **Locomoção (estágio 1):** extrair composição local e crédito diagnóstico;
   manter Player igual; versionar novas saídas do Enemy e preservar execução
   dos cérebros antigos via adaptador ou fallback até substituição aprovada.
   Treinar objetivo, obstáculos, corredores e portas com colisão real. Testar
   velocidade cardinal/diagonal, recuo, strafe, simetria, paredes e travamento.
3. **Mira (estágio 2):** registrar erro angular e tempo de aquisição/seguimento
   em alvo parado, velocidades crescentes, mudança de direção, strafe, oclusão
   e distâncias distintas. Verificar que giro não premia órbita infinita.
4. **Tiro (estágio 3):** primeiro decidir se o ataque inimigo continuará como
   dano direto/hitscan ou terá eventos de tiro com munição/recarga. Só depois
   treinar precisão real, rajadas, desperdício e recarga. Atualizar combate,
   sensores e formato de cérebro de modo compatível com saves/campanha.
5. **Esquiva (estágio 4):** para hitscan, treinar reação preventiva a ameaça
   observável e cobertura; para projéteis futuros, medir esquiva contra um
   ataque que atingiria a trajetória anterior (contrafactual), sem bônus por
   strafe sem ameaça. Incluir avanço/recuo diagonal e laterais.
6. **Combate e posição (estágios 5–6):** 1x1 simples, depois faixa ideal,
   cobertura, exposição, flanco e aliados. Usar perfis reais; impedir vitória
   por mera sobrevivência passiva ou por fallback roteirizado.
7. **Combate real e promoção (estágio 7):** amostrar mundos/portas/corredores,
   posições e ações do Player distintos, inclusive encadeamento de saltos,
   air-strafe e velocidade do Bhop. O treino atual reloca o Player diretamente
   em trajetórias sintéticas; para representar Bhop, usar o movimento físico
   real nos novos cenários, sem teletransportá-lo a cada tick. Separar famílias
   de mapas e padrões entre treino, validação fixa e avaliação aleatória.
   Comparar com cérebro
   distribuído e IA roteirizada nas mesmas seeds e no jogo visual. Só promover
   um candidato após métricas e playtest satisfatórios; manter rollback.

Cada estágio tem testes de invariantes, determinismo e métricas do arquétipo.
Definir limites de aprovação quantitativos **após** medir uma baseline com os
novos eventos; um reward maior sozinho não é critério de promoção.

## 5. Evidência de referência e validação necessária

- `tests/test_gameplay.cpp` já cobre normalização diagonal do Player.
  `tests/test_enemy_sensors.cpp` cobre movimento frontal/lateral positivo e
  navegação do boss, mas não um checkpoint que pede recuo inoportuno.
- `tests/test_worker_training.cpp` cobre direção de alguns termos de fitness,
  determinismo, checkpoint e currículo. Falta teste de crédito neural versus
  fallback, recompensa telescópica, anti-oscilação contextual e escala relativa
  dos termos terminais.
- `tools/check_ai_gameplay_eval.cmake` verifica duas execuções idênticas e
  presença de linhas, não qualidade. O modo padrão tem dano, hits, deslocamento,
  menor/distância final e ticks neurais. O modo `--duel`, implementado depois
  desta auditoria, acrescenta tiros reais do Player, desfecho, dano bilateral,
  saltos e velocidade por física real; ainda faltam exposição, cobertura,
  colisões, reversões, autoria do movimento e várias métricas propostas.
- Os checkpoints de 10.000 gerações permanecem experimentais. A validação
  registrada em `animation_and_ai.md` encontrou regressão de combate frente
  aos assets atuais em 10 seeds. Como os currículos/holdouts podem diferir,
  não comparar fitness antigo e novo como se fossem a mesma escala.

**Verificação de 2026-09-29:** com `gdp_ai_gameplay_eval`, seed 12345, 360
frames, o boss com `assets/ai/robot.brain` causou 32 de dano no cenário
`boss_cover`, enquanto o checkpoint experimental de geração 10.000 causou 0.
Esse é um cenário controlado, não uma partida completa nem prova isolada de
qual política é melhor no geral.

Os testes locais `gameplay_systems`, `enemy_sensors_systems`,
`worker_training_systems` e `ai_gameplay_eval_deterministic` passaram (4/4).
Eles validam o comportamento existente e a reprodutibilidade, não os estágios
novos propostos neste documento.

**Primeiro passo do estágio 0 (2026-09-29):** `gdp_ai_gameplay_eval --duel`
compara as duas políticas para os cinco arquétipos em `ground_strafe` e `bhop`.
O Player usa `Player::fixedUpdate`, sem relocação por tick, e atira com
`fireHitscan`; a arma/mira são fixas para comparar as políticas sob condições
iguais. O teste automático exige 20 linhas determinísticas, tiros/acertos,
múltiplos saltos e velocidade de Bhop superior à de caminhada. Isto **não**
encerra o estágio 0: faltam telemetria por decisão, cobertura/portas e uma
amostra robusta de seeds. Nenhum reward, formato de cérebro ou asset promovido
foi alterado.

**Regras de treino v2 (2026-09-29):** o módulo
`src/game/ai/TrainingReward.cpp` passou a aplicar terminal `+10/-10` e teto
de `±4` por episódio para todos os termos auxiliares. Dano causado, acerto e
eliminação durante fallback são registrados à parte e não rendem fitness neural; suporte
automático não rende fitness. Dano recebido durante fallback ainda penaliza a
sobrevivência do episódio. `approachMeters` foi substituído por progresso
assinado na faixa tática, limitado pelo progresso líquido total para não
premiar ciclos. Reversões sem melhora de faixa ou mira, quando não há disparo
iminente do Player, recebem penalidade. Atribuição proporcional de avanço,
recuo e strafe é apenas diagnóstica, separada da velocidade física. O currículo
tem agora cinco cenários de treino e dois de holdout por arquétipo; um cenário
de treino usa Bhop pela física real do Player. O log marca `reward_rules=2`.
Os cérebros ativos, os sensores e o formato `.brain` **não mudaram**; scores
antigos e novos não devem ser comparados diretamente. Ainda faltam os estágios
isolados de mira/tiro/esquiva e geometrias de corredor/porta. Como o ataque
inimigo não tem evento de tiro, munição ou projétil, não foram inventadas
recompensas por precisão, recarga ou esquiva de tiro já disparado. Esta é a
fotografia anterior ao checkpoint de projéteis abaixo.

**Contrato tático v3 (em implementação):** treinos novos geram cérebros com
12 entradas, 12 neurônios ocultos e 5 saídas. Os sete sensores antigos mantêm
seus índices; os novos registram velocidade visível do Player em eixos locais,
ameaça de mira, munição e recarga. As saídas passam a representar frente,
strafe independente, giro, ataque e recarga. O arquivo `GDP_ENEMY_BRAIN 3`
registra o contrato e seu checksum. Arquivos v1/v2 continuam carregáveis com
movimento legado, mas não servem como `--resume` do formato novo. A ação de
recarga e os eventos de projétil ainda precisavam ser ligados ao combate neste
checkpoint; o passo seguinte faz essa ligação.

**Projéteis inimigos (checkpoint seguinte):** Guard, Officer, Engineer e Robot
agora lançam projéteis visíveis, com velocidade finita, colisão com cobertura,
munição e recarga. O tiro guarda sua direção no disparo: deslocamento posterior
do Player pode evitá-lo. Worker permanece melee. O motor de simulação é o mesmo
no jogo, no treino e no avaliador de gameplay; o treino atribui acerto e dano
ao autor do disparo após contato, mesmo que o motor tenha entrado em fallback
no intervalo. Tiros bloqueados e oportunidades de esquiva são eventos, não
pedidos de ataque. O Player ainda dispara hitscan, então a reação defensiva
dos inimigos no jogo continua preventiva. O currículo em sete estágios e
avaliação em corredores/portas permanecem a etapa seguinte; ainda não houve
promoção de cérebro. A penalidade limitada por erros/obstruções altera o log
para `reward_rules=3`; scores v2/v3 não são comparáveis diretamente.

**Currículo em sete estágios (checkpoint atual):** o treino libera por geração
locomoção, mira, controle de tiro, esquiva, duelo, posicionamento e mapa real.
As etapas 1–2 desligam dano para isolar movimento e mira; a 3 exercita tiros,
munição e recarga sem ataque do Player; a 4 lança projéteis de treino contra
o inimigo e só credita esquiva quando uma trajetória que o ameaçava passa sem
acerto; as etapas 5–7 combinam o combate. O estágio 7 gera um mundo com seed
independente, usa a colisão de navegação do jogo e tenta iniciar o inimigo em
um corredor conectado. O holdout percorre todos os sete estágios com outras
seeds; `--all-scenarios` amplia de duas para quatro seeds por estágio. O log
inclui autoria neural/fallback, tiros, acertos, recargas, oportunidades e
esquivas, exposição/cobertura, distância, bloqueio, Bhop e casos de mapa.
As pontuações específicas das etapas iniciais não são comparáveis entre
gerações: `stage=N/7` informa o que já foi liberado. Sete gerações ou mais
são necessárias para completar o ciclo. O ataque do Player no jogo continua
hitscan; a etapa de esquiva com projéteis testa reação antecipada à mira e
trajetória, mas não prova esquiva pós-disparo em gameplay. Ainda são exigidos
comparação pareada com cérebros antigos e playtest antes da promoção.

**Paridade de execução do treino (checkpoint seguinte):** o treino agora usa
`controlAll=true`, assim como o jogo. Cada inimigo mantém seu próprio estado
motor e a telemetria consulta as saídas desse estado, não uma saída global.
Isso inclui as salvaguardas de mira e o fallback temporário de produção na
simulação. O log marca `runtime_control_all=1` e os currículos `8`/`9`.
Checkpoints obtidos antes dessa correção são diagnósticos e não são candidatos
à promoção, mesmo que o fitness de treino pareça melhor.

**Rodada curta com paridade (2026-09-29):** cinco treinos independentes, seed
42, população 8, 14 gerações por arquétipo, sete estágios concluídos, sem
pausa artificial. Os checkpoints e o log estão localmente em
`build/ai-runs/tactical-runtime-20260929/` e
`build/ai-runs/tactical-runtime-20260929.log` (arquivos de build ignorados pelo
Git). Os holdouts finais, na escala nova, foram Worker `-3,017`, Guard `0,621`,
Officer `0,564`, Engineer `-0,509` e Robot `-0,343`. Esses números **não**
autorizam promoção por si só.

Comparação de gameplay em três seeds não usadas diretamente na seleção
(`12345`, `67890`, `54321`), 360 frames por cenário, candidato da geração 14
contra o cérebro ativo do mesmo tipo. Os números abaixo somam o dano no Player;
o teste padrão tem quatro cenários por tipo, mais `boss_cover` para Robot, e
o duelo tem `ground_strafe` e `bhop`:

| Tipo | Padrão ativo → novo | Duelo ativo → novo |
| --- | ---: | ---: |
| Worker | 280 → 210 | 30 → 30 |
| Guard | 732 → 708 | 96 → 24 |
| Officer | 462 → 468 | 42 → 42 |
| Engineer | 480 → 465 | 35 → 25 |
| Robot | 780 → 588 | 170 → 174 |

Todos os seis duelos por tipo mantiveram o mesmo desfecho entre ativo e novo:
vitória do Player para os quatro tipos comuns; limite de tempo para Robot.
No cenário `boss_cover`, Robot caiu de 32 para 0 de dano nas seeds `12345` e
`67890` (e também na terceira seed). A queda do Guard contra Bhop/strafe e
a cobertura do boss tornam a substituição insegura. **Nenhum dos cinco assets
em `assets/ai/` foi trocado.** Antes de uma nova rodada, priorizar locomoção
em cobertura/corredor e avaliação visual de orientação, sem usar o fitness
como único critério.

Validação local desta fase: build Debug concluído e `ctest --test-dir
build/debug --output-on-failure` passou em 59/59 testes. Isso confirma as
invariantes automatizadas e não substitui uma partida visual completa.

**Playtest isolado dos candidatos:** `--test-brain-dir DIR` carrega todos os
cinco cérebros de `DIR/{worker,guard,officer,engineer,robot}.brain`, exigindo
arquivos válidos do tipo correspondente. Os cinco checkpoints da geração 14
foram copiados somente para
`build/ai-test/tactical-runtime-20260929/`; `assets/ai/` permanece intacto.
Executar `build/debug/gdp --test-brain-dir
build/ai-test/tactical-runtime-20260929` abre o jogo com os candidatos, e
executar `build/debug/gdp` volta à IA ativa. Uma inicialização headless de
três frames confirmou os cinco carregamentos como geração 14. A comparação
curta nas três seeds da tabela acima foi repetida com essa pasta e reproduziu
os mesmos totais. Trata-se de teste jogável, **não de promoção**: observar
especialmente mira do Guard contra Bhop/strafe e navegação do Robot atrás de
cobertura antes de decidir por qualquer substituição permanente.

**Ajuste de seleção após a rodada curta:** um primeiro treino de sete gerações
mostrou que boa locomoção nas primeiras etapas não conserva necessariamente
a capacidade de atirar nas últimas. O currículo 6 (7 com quatro seeds por
estágio) reintroduz dois candidatos táticos, de direções laterais opostas,
quando um estágio é liberado; seus pesos respeitam o limite do arquivo
`.brain`. A primeira comparação pareada em `gdp_ai_gameplay_eval --duel`,
seed 12345 e 360 frames, mostrou regressões dos candidatos da rodada v2
em Guard, Engineer e Robot contra Bhop. Esses candidatos não foram promovidos.

**Regras de treino v4 (após a rodada até a geração 114):** a inspeção da rodada
longa mostrou que o Worker deixou de causar dano para evitar mortes; o Engineer
melhorou na esquiva, mas manteve poucos acertos; e o Robot não esquivou de
nenhum disparo sintético nas 100 gerações observadas. O treino agora limita a
nota de episódios de duelo/posicionamento/mapa real sem acerto neural para
Worker e Engineer. Na seleção depois de liberar os sete estágios, esses dois
tipos precisam acertar em pelo menos uma repetição equivalente de episódios de
combate; sem isso, recebem uma nota de inelegibilidade inferior a qualquer
episódio normal. Aproximação e contato não compensam a ausência de ataque.
Para Worker, o tempo de contato passa a exigir a distância real do golpe
(1,15 m), em vez de até 4 m. Esses limites não são aplicados às etapas
iniciais de locomoção/mira, nas quais atacar está desabilitado.

O disparo sintético do estágio de esquiva foi reduzido a 6 m/s somente contra
Worker e Robot: com os raios, velocidades de movimento e distâncias iniciais
desses tipos, os 13 m/s anteriores não davam tempo lateral suficiente para
evitar um tiro apontado ao centro. Isso **não** muda o tiro hitscan do Player no
jogo nem prova esquiva pós-disparo; é um exercício de antecipação à mira. Guard,
Officer e Engineer mantêm 13 m/s. As seeds de cada estágio usam uma posição
fixa por cenário e repetição, de modo que as duas primeiras repetições do modo
normal e do `--all-scenarios` coincidam. O log v4 registra, por estágio,
pontuação, episódios com acerto e esquivas/oportunidades, além da elegibilidade
de combate na geração e no holdout. Na linha `AI_GENERATION`, esses campos
por estágio descrevem a reavaliação completa do campeão (`full_score`), mesmo
quando a seleção da geração ainda usa menos estágios (`stage=N/7`). Comparar
políticas com o mesmo build,
quantidade de repetições e seeds; scores v3 e v4 não são comparáveis. O formato
dos cérebros e os assets ativos permanecem iguais. Nenhuma promoção deve
ocorrer sem comparação pareada de gameplay e playtest visual.

Cada geração v4 deriva uma nova seed de situações a partir da seed principal e
do número absoluto de geração. Todos os candidatos daquela geração veem os
mesmos cenários, preservando uma seleção justa, mas a sequência percorre mapas
e trajetórias diferentes em vez de decorar uma única amostra por horas. O log
inclui `scenario_seed`; ao retomar um checkpoint, o mesmo número de geração
reproduz a mesma amostra. O holdout fica fixo e independente. Por causa dessa
rotação, uma subida ou queda isolada de `train_best` entre gerações não mede
melhora de política; usar holdout pareado e gameplay para decidir promoção.

Para dar um sinal de aprendizagem antes da primeira esquiva completa, o estágio
de Dodge também mede a distância **lateral à trajetória do disparo que acertou**
durante o controle neural. Esse crédito parcial é limitado a 0,7 ponto por
episódio (contra a penalidade de 0,45 por acerto); uma esquiva efetiva ainda
vale mais. Andar lateralmente sem projétil, ou um disparo bloqueado pelo cenário,
não gera esse crédito. O log informa `dodge_lateral_m` e
`holdout_dodge_lateral_m`. Trata-se de recompensa no exercício sintético, sem
alterar o dano ou os projéteis no jogo.

**Rodada diagnóstica v4, sem promoção (2026-09-30):** retomando o checkpoint
do Robot da geração 114 por 20 gerações, população 8, seed 42 e duas repetições
por cenário, o campeão da geração 134 fez 11 esquivas em 16 oportunidades no
holdout fixo, com `holdout=1.280` e 30 acertos neurais. A rodada curta anterior
à recompensa lateral terminara sem esquivas (0/19–20 por geração). A comparação
de gameplay em seed 12345 por 360 frames, entretanto, não confirmou melhoria
geral: dano igual em área aberta (48), menor contra jogador elevado (36 contra
48) e ambos os checkpoints sem acerto no cenário de cobertura do boss. A
esquiva do exercício sintético é promissora, mas **não autoriza trocar o cérebro
ativo**. No duelo bilateral da mesma seed e 360 frames, os dois tiveram um
acerto no jogador em solo (12 de dano) e quatro contra Bhop; o candidato novo
causou 42 contra 44 de dano no Bhop. Faltam mais seeds e partida visual para
julgar navegação, precisão e balanceamento.

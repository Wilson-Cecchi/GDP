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
  presença de linhas, não qualidade. A saída atual tem dano, hits, deslocamento,
  menor/distância final e ticks neurais; faltam as métricas pedidas acima.
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

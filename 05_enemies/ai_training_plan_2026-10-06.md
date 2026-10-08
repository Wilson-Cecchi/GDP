# Próximo ciclo de treino dos cérebros — plano de 06/10/2026

## Decisão

Criar um protocolo de treino e avaliação novo, isolado do serviço atual. O
objetivo é selecionar políticas que combatam melhor em cenários variados e
manter a qualidade entre gerações. Aumentar apenas o número de gerações não é
o primeiro experimento: o serviço já chegou a aproximadamente 26 mil gerações
por arquétipo e ainda oscila.

Não há evidência, neste momento, de que o tamanho da rede (12 entradas, 12
neurônios ocultos e 5 saídas) seja o limite principal para o combate
individual. Preservar esse contrato na primeira rodada permite isolar o efeito
da avaliação, do currículo e da seleção. O objetivo adicional de combate em
equipe exige outro contrato: o cérebro atual não observa aliados nem o risco
de acertá-los. Desenvolver esse contrato em uma trilha separada, com comparação
controlada ao combate individual.

## Evidência que orienta o plano

- Validação independente de 06/10, oito seeds novas e 1800 frames: Worker
  26460 passou com score 1,057 e zero regressões; Guard 0,876/11, Officer
  0,832/25, Engineer 1,067/9 e Robot 0,864/20 foram reprovados. O Engineer
  ganhou na média dos mapas e perdeu em casos específicos e no Bhop.
- Fitness do mesmo lote não prediz aprovação: Worker −20,031, penalizado por
  acertar em três de quatro episódios exigidos, passou fora do treino;
  Robot 1,454 teve o maior fitness interno e foi reprovado.
- Nas dez baterias amplas mais recentes por tipo, passaram Worker 5/10,
  Guard 1/10, Officer 0/10, Engineer 0/10 e Robot 0/10. São resultados de
  desenvolvimento, não estimativas de sucesso na campanha.
- O treino atual escolhe pais por fitness de uma bateria determinística que
  muda a cada geração. Usa 75% da média de episódios e 25% do pior episódio;
  Worker e Engineer podem receber penalidade binária próxima de −20 quando
  ficam abaixo do mínimo de episódios com acerto. A avaliação de gameplay
  acontece ao fim do lote e arquiva candidatos, sem influir na escolha de
  pais da geração seguinte.
- No avaliador atual, o dano em mapas satura em 100 e o Player não atira.
  Nos duelos, o Player atira com mira automática; todos os 16 duelos de cada
  tipo na última validação terminaram com vitória do Player. A fixture de
  grupo contém três inimigos do mesmo arquétipo, então não mede comando e
  reparo com aliados diferentes. A altura de teste do avaliador é maior que
  a plataforma baixa acessível ao Worker no currículo.

Código principal: `src/game/ai/WorkerTraining.cpp`,
`src/game/ai/TrainingReward.cpp`, `src/game/ai/TrainingEncounter.cpp`,
`tools/ai_gameplay_eval.cpp` e `tools/zimaos/`.

## Etapa 0 — congelar o ponto de partida

1. Registrar imagem e código atuais, hashes dos cinco assets ativos, melhor
   candidato arquivado por tipo, campeão recente, população completa mais
   recente, parâmetros e relatórios. Preservar a população para retomada e
   garantir rollback do serviço atual.
2. Registrar uso de disco, temperatura e desempenho do ZimaOS. Verificação
   em 06/10: contêiner saudável, 0,75 CPU/512 MiB configurados, 55 °C e
   cerca de 405 GiB livres em `/DATA`. Conferir novamente no corte.
3. Definir namespaces e manifestos de seeds separados: treinamento, seleção
   de desenvolvimento e confirmação final inédita. As seeds finais das
   avaliações de 02, 04 e 06/10 já foram consultadas e não servirão de
   confirmação independente do protocolo novo.

**Entrega:** baseline reproduzível e snapshot validado da população atual.

## Etapa 1 — tornar o avaliador discriminante

1. Manter a bateria antiga como série histórica. Criar `evaluation_v3` em
   paralelo, com identificação de protocolo, revisão, seed, duração, hashes,
   cenário e resultados brutos por par.
2. Calibrar duas dificuldades de duelo usando os cérebros ativos, sem mudar
   o combate da campanha: uma que permita observar sobrevivência/derrota de
   ambos os lados e outra de estresse contra Bhop. Registrar vitórias,
   tempo até morte, dano causado/recebido, tiros, bloqueios, recargas e ticks
   neurais/fallback. Não inferir vitória de dano médio maior.
3. Em mapas, acrescentar tempo para derrotar o Player e marcos de dano antes
   do teto de 100. Cobrir campo aberto, cobertura central/lateral, peek,
   corredor/porta, fase do boss e alturas fisicamente alcançáveis por cada
   tipo. Validar spawn, colisão e visão.
4. Adicionar fixtures de grupo misto com Guard ferido para Engineer e com
   aliados comandáveis para Officer. Medir comandos/reparos efetivos e dano
   do grupo sem dar ao cérebro crédito por suporte automático não escolhido.
5. Fixar uma bateria sentinela de desenvolvimento, acrescentar geometrias e
   seeds aleatórias independentes e reservar uma bateria final fechada.
   Calibrar metas de vitória/duração pelos resultados dos ativos antes de
   conhecer candidatos novos.

**Gate:** relatórios completos e determinísticos, diferença útil entre
políticas conhecidamente fortes/fracas e ausência de cenários inviáveis.
Testar que um ganho de score não oculte falha grave por seed ou cenário.

## Etapa 2 — reduzir ruído na escolha dos pais

1. Preservar todos os indivíduos de uma geração no mesmo painel de treino,
   como hoje, mas amostrar famílias de situações de forma estratificada:
   duelo em solo/Bhop, cobertura/peek, aberto, corredor, altura adequada e
   fases do Robot. Garantir um mínimo por família em cada comparação.
2. Reavaliar os melhores indivíduos e o incumbente em um segundo painel de
   desenvolvimento, idêntico para ambos, antes de escolher os pais. Usar
   média por família e um termo limitado para o pior grupo; registrar a
   dispersão entre seeds. O painel deve variar de forma determinística ao
   longo do treino sem usar as seeds finais.
3. Substituir a penalidade binária de competência do Worker/Engineer por uma
   função graduada ligada a acerto, dano e oportunidade real de ataque.
   Testar explicitamente 0, 1, 3 e 4 episódios com acerto e episódios em que
   atacar é fisicamente impossível. Preservar autoria neural/fallback e
   limites dos termos auxiliares da recompensa.
4. Arquivar tanto o melhor fitness quanto o melhor candidato de combate
   dentro do protocolo; voltar a inserir uma política arquivada na população
   apenas por regra explícita e testada, evitando que uma seed recente apague
   um comportamento geral melhor. Não usar a bateria final para seleção.

**Gate:** teste de continuação determinística da população inteira, teste de
ausência de crédito por fallback/suporte automático e demonstração de que a
política preferida supera o incumbente no segundo painel. Versionar regras de
recompensa, currículo e metadados da população; não interpretar `reward_rules=5`
como se fossem regras novas.

## Etapa 3 — currículo e experimentos por papel

Manter os sete fundamentos existentes e variar os encontros de combate com
objetivos por arquétipo:

| Tipo | Falha prioritária | Critério observado |
|---|---|---|
| Worker | Perder ataque em campo aberto ou orbitar sem contato | Acertos, tempo até contato e vitória; altura somente quando alcançável |
| Guard | Cobertura central e Player pulando | Dano e tiros efetivos por seed, tempo sem visão, reposicionamento |
| Officer | Queda em cobertura, altura e campo aberto | Dano próprio e efeito de comando em grupo misto |
| Engineer | Bhop e perdas individuais apesar da média boa | Dano por cenário e reparo efetivo em aliado ferido |
| Robot | Campo aberto, duelo em solo e oscilação entre lotes | Dano, vitória/duração e três fases de vida |

Executar um piloto com Worker como controle de regressão e Officer/Robot como
casos difíceis: duas seeds de treino por braço e 100 gerações por seed. Comparar
`v2` e `v3` a partir do mesmo campeão `.brain` congelado por papel: cada braço
cria uma população nova de 8 com a mesma seed inicial e a mesma regra de
mutação. Registrar o campeão escolhido antes do piloto. Não carregar a
população completa de `reward_rules=5` sob regras novas como se o fitness
salvo ainda fosse válido. Manter CPU, duração e avaliador iguais. Se a mudança
funcionar, repetir nos cinco tipos com três
seeds por braço e cerca de 300 gerações por seed. Esses tamanhos são orçamentos
iniciais de experimento, ajustáveis depois de medir variância e tempo.

Testar mudanças isoladas: primeiro avaliador e seleção; depois currículo;
depois recompensa. Só testar população 16, mutação diferente ou sensores
novos após esses controles. O piloto anterior de 7 gerações não demonstrou
ganho consistente de população 16.

**Gate:** a variante nova supera o controle em seeds de desenvolvimento de
mais de uma execução e reduz os modos de falha por papel. Uma melhora apenas
em fitness ou só na média não basta.

## Etapa 3B — inteligência por dificuldade e combate em equipe

Preservar a estrutura e o ritmo geral do gameplay. As opções Easy, Normal e
Hard já existem, mas hoje alteram velocidade e dano dos inimigos, não o que
eles percebem ou como decidem. Acrescentar um perfil de decisão à dificuldade
salva na partida, sem mudar as opções da interface:

| Nível | Comportamento alvo |
|---|---|
| Easy | Reação individual mais lenta, busca simples e erros ocasionais de posicionamento. Evita um aliado claramente na linha de tiro, sem coordenação elaborada. |
| Normal | Compartilha a última posição observada do Player com aliados próximos, procura outra linha de tiro quando há obstrução e alterna pressão com cobertura. |
| Hard | Compartilha informações observadas com atraso limitado; distribui ângulos de ataque, flanqueia e usa melhor os papéis de Officer e Engineer. Mantém limites físicos de percepção, munição e recarga. |

Não dar a nenhum nível a posição atual do Player quando ele estiver oculto.
Informação de equipe vem de visão ou ruído efetivamente percebidos, tem prazo
de validade e alcance definido. O compartilhamento de alvo, a escolha de
ângulo e a decisão de atirar devem resultar em comportamento legível no jogo,
sem telepatia nem imobilizar o grupo esperando uma formação perfeita.

1. **Regra física única para todos os níveis:** o projétil inimigo atinge o
   primeiro obstáculo, Player ou inimigo vivo no trajeto. Ignora somente o
   atirador, aplica dano real ao aliado e se encerra no impacto. Usar a mesma
   regra para disparos neurais e de fallback; cobrir inimigos de salas
   adjacentes onde o projétil possa atravessar. Definir resolução determinística
   do primeiro impacto e preservar colisão com cenário e possibilidade de
   desviar. O melee do Worker fica fora deste primeiro corte.
2. Acrescentar ao evento do projétil alvo, tipo de impacto e dano para medir
   acertos aliados. Reusar o fluxo normal de dano/morte do inimigo. Morte por
   tiro aliado deve liberar o encontro e resolver objetivos/boss como qualquer
   morte; a XP por abate continua reservada a abates creditados ao Player.
   Verificar efeitos visuais/sonoros e regras de loot antes da ativação.
3. Antes de treinar, introduzir disciplina de fogo: detectar aliado na faixa
   provável do disparo e permitir segurar o tiro ou buscar ângulo livre. Esta
   decisão não torna o aliado invulnerável: movimento e dispersão ainda podem
   produzir fogo amigo. Evitar um veto global que faça todos pararem de atirar.
4. Criar um contrato `.brain` novo e versionado para cooperação, com entradas
   observáveis como direção/distância do aliado mais relevante, risco de linha
   de tiro, posição conhecida compartilhada e papel do grupo. Manter carregamento
   dos contratos antigos e não atribuir ao cérebro ações automáticas do Officer
   ou Engineer. Começar com poucas entradas mensuráveis; só adicionar saídas
   novas se as cinco atuais (movimento, giro, ataque e recarga) não permitirem
   executar a tática desejada. O coordenador de grupo fornece observações e
   objetivos locais; a política decide mover, atacar ou esperar.
5. Treinar grupos mistos, não apenas três inimigos iguais: Guard + Officer,
   Engineer + Guard ferido, pressão frontal com flanqueador, corredor estreito,
   cobertura, portas e Player mudando de posição. A recompensa deve considerar
   dano/vitória contra o Player, sobrevivência do grupo, acerto em aliados e
   oportunidade de tiro desperdiçada. Penalizar fila que obstrui a própria
   equipe, mas também penalizar a solução trivial de nunca disparar. Crédito
   individual e de equipe precisam ser separados nos relatórios.
6. Selecionar políticas por perfil de dificuldade somente após avaliação
   pareada e playtest. O perfil de Hard não será simplesmente o último cérebro
   treinado; políticas para os três níveis precisam cumprir seus critérios de
   comportamento. Manter os `.brain` ativos até que candidatos tenham passado
   pelos gates. Se for mais simples inicialmente, usar uma política condicionada
   pelo perfil e calibrar sua reação/compartilhamento; três conjuntos de assets
   só se houver ganho mensurável que compense sua manutenção.

**Avaliação isolada:** comparar perfis com os mesmos inimigos, mapas, seeds,
vida, armas e dano para medir apenas inteligência; depois repetir com os
multiplicadores Easy/Normal/Hard reais. Medir vitórias, tempo de sobrevivência
do Player, dano causado/recebido, tiros retidos, mudanças para linha livre,
acertos e mortes por fogo amigo, uso de flanco/suporte, tempo ocioso, casos de
empilhamento e custo de CPU. Exigir que Hard seja mais competente que Normal,
e Normal que Easy, em vários cenários, sem tornar cada encontro uma vitória
certa ou recompensar fogo amigo. Fixar limites após calibrar o comportamento
atual e antes de selecionar candidatos.

**Ordem de entrega:** colisão e telemetria do fogo amigo, com testes de primeiro
impacto e morte; percepção/coordenação de grupo com testes de informação
oculta e persistência da dificuldade; avaliador e currículo de equipe; piloto
curto no ZimaOS isolado do treino atual; playtest e eventual promoção. Alteração
do comportamento na campanha ou no Speedrun requer revisar versões de regras
e comparabilidade dos recordes antes da ativação.

**Gate:** cada nível mostra a competência esperada em grupos mistos e também
em duelos individuais; o combate continua resolúvel para o Player; fogo amigo
tem efeito físico e é raro quando existe uma linha livre alternativa; nenhum
cérebro recebe informação oculta; o desempenho cabe no orçamento de frame.

## Etapa 4 — confirmação, implantação e rollback

1. Congelar um candidato por tipo e perfil aprovado segundo o desenvolvimento,
   com hash, revisão e protocolo. Comparar cada um ao ativo e ao melhor
   arquivado do ciclo anterior, no mesmo binário e nas mesmas seeds finais
   inéditas. A trilha de equipe só participa da promoção após passar seu gate.
2. Exigir ausência de regressão grave nos cenários sentinela, relatar os
   piores pares e a distribuição das seeds aleatórias, e exigir melhora
   observável em dano, tempo ou vitória quando as métricas não estão no teto.
   Os limiares definitivos da bateria v3 serão fixados após calibrar os ativos
   na Etapa 1 e antes de avaliar candidatos.
3. Fazer playtest no jogo para leitura do combate, navegação, suporte e fases
   do boss. Só então considerar troca de `assets/ai/*.brain`, com os hashes
   antigos guardados para rollback.
4. Para o ZimaOS, construir imagem nova com tag/revisão, diretório de dados
   novo e manifesto de migração. Pausar o serviço velho no corte, validar
   checkpoints e iniciar o novo como processo único. Conservar limites de
   CPU/memória e proteção térmica 80/70 °C; confirmar healthcheck, logs,
   resume após parada e retorno automático após reinício. Se falhar, restaurar
   a imagem e a população antigas sem alterar os assets ativos.

## Arquivos e critérios de trabalho

- Avaliação: `tools/ai_gameplay_eval.cpp`,
  `tools/zimaos/training_evaluation.py` e testes de cenários/CSV.
- Treino: `src/game/ai/WorkerTraining.cpp`, `TrainingReward.cpp`,
  `TrainingEncounter.cpp`, `TrainingPopulationStore.cpp/.hpp` e testes C++ de
  determinismo/continuação.
- Serviço: `tools/zimaos/train_service.py`, scripts de construção e manual de
  operação; protocolo novo isolado do `quick-v2`/`wide-v2` atual.
- Cada entrega pequena deve registrar revisão, seeds, hashes, resultados
  pareados, regressões e custo por geração. Uma mudança no contrato dos
  sensores/ações exigirá formato `.brain` novo e carregamento compatível dos
  arquivos antigos.

**Estado em 07/10:** implementação local em andamento. O projétil inimigo
causa dano a aliados, informa alvo/abate, e o atirador segura o disparo e
procura outro ângulo quando um aliado bloqueia a linha. A dificuldade da
sessão controla intervalo de decisão neural, velocidade do desvio e
alcance/validade da informação compartilhada por avistamento. O
`currentSpeedrunRulesVersion` passou de 38 para 39 para separar recordes
sob regras de combate diferentes.

O contrato cooperativo 3 usa 19 sensores, inclusive aliado próximo, faixa de
tiro e memória compartilhada recente; o formato `.brain` 4 mantém leitura dos
contratos anteriores. O avaliador `--team` tem cenários mistos com fogo amigo,
suporte e dano por origem. O treino opt-in `--cooperative-brain` usa episódios
com um aliado ativo, métricas separadas por atirador, penalidade para dano em
aliados e espera prolongada por linha de tiro, `reward_rules=6`, além de
snapshot de população versionado para retomada. A trilha Tactical continua
com `reward_rules=5` e arquivos compatíveis.

Smoke local de 07/10 (`Guard`, seed 42, população 2, uma geração, currículo
1/7): 14 episódios de grupo na avaliação do melhor indivíduo, 7 acertos em
aliados, 56 de dano aliado e 794 retenções de tiro. O holdout marcou 12
acertos aliados e 96 de dano. É apenas verificação de execução e telemetria;
esse cérebro não passou pelo currículo de combate nem pelo gate de promoção.

Piloto exploratório separado em `build/debug/coop-pilot-guard-20261007`
(`Guard`, seed 20261007, população 4, duas gerações, sete etapas desbloqueadas
na segunda, 4 repetições por etapa): holdout −1,150, 9 acertos e 72 de dano
em aliados nos 28 episódios de grupo. Na avaliação `--team` com seeds 9001 e
9002, 480 frames e os mesmos aliados ativos, o dano somado ao Player caiu de
181 (Guard ativo) para 133 (candidato). Ambos tiveram vitória do Player no
cenário `firing_lane` nas duas seeds. O candidato foi reprovado; os CSVs e os
checkpoints permanecem apenas no diretório de build para inspeção.

Faltam calibrar as três dificuldades em cenários pareados, ampliar encontros
de equipe para flanco/corredor/porta, executar piloto isolado, medir custo por
frame e fazer playtest. O treino v2 no ZimaOS e os cérebros ativos não foram
modificados nesta implementação local.

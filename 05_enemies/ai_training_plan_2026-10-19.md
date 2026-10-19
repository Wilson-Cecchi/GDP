# Treino cooperativo de percepção — protocolo 3

Este ciclo usa o código de combate atual e acrescenta aviso de aliado por
evento de aquisição ou recuperação de visão. O aviso contém a última posição
vista arredondada a uma grade de 2 m; cada receptor guarda a origem e a idade
da informação. A posição não é atualizada enquanto a visão permanece contínua.
O currículo 22 treina os cinco arquétipos com perda de visão, investigação e
recuperação. O Robot também recebe um encontro com ajudantes e a rajada atual.

## Isolamento

- Fonte congelada da revisão r2: `433f51b5eb64+worktree-747ed7d9d9988fa3`. O sufixo
  identifica o conteúdo completo do snapshot, que inclui alterações locais
  ainda não commitadas.
- Imagem: `gdp-training:perception-v3-r2-20261019`.
- Serviço: `gdp-perception-v3-r2` em `/DATA/AppData/gdp-perception-v3-r2`.
- Dados: `/DATA/AppData/gdp-perception-v3-r2/data`; nenhuma população dos
  currículos 19–21 é importada. O serviço exige população completa do
  currículo 22 para retomar.
- O serviço r2 fica parado com política `restart=no`; seus dados permanecem em
  `/DATA/AppData/gdp-coop-new-mechanics-r2/data`.
- A primeira revisão v3 (`433f51b5eb64+worktree-1c230d1f49866bd4`) também
  permanece parada e preservada em `/DATA/AppData/gdp-perception-v3`. Seu
  gate ainda aceitava um candidato que não entrava no estado de investigação.
  A revisão r2 corrige a classificação da IA neural e exige investigação
  observável dos dois inimigos no fixture.

## Avaliação

Cada candidato é comparado aos cérebros ativos com o mesmo binário, sementes
e duração, em mapas, duelos, equipes com Boss e no fixture `lost_sight`. O
fixture registra origem e idade do aviso, investigação, movimento e três
contadores de vazamento de informação. Qualquer atualização de posição sem
visão, contato visual após ocultação ou reinício da idade sem nova aquisição
reprova o candidato. O relatório combina os gates de combate e percepção sob
o protocolo 3; arquivos aprovados ficam apenas em `data/candidates/`.

As sementes rápidas são 152201 e 152213 (480 quadros); a avaliação ampla
acrescenta 152227 e 152239 (1200 quadros). São sementes de desenvolvimento.
Uma avaliação final deve usar sementes inéditas e playtest antes de qualquer
promoção para `assets/ai/*.brain` ou para a versão de Download.

## Monitoramento

```sh
ssh zima 'cd /DATA/AppData/gdp-perception-v3-r2 && docker compose -f compose.perception-v3-r2.yaml ps && cat data/status.json && cat data/state.json'
```

Os logs de execução e os checkpoints ficam sob `data/logs/`, `data/runs/` e
`data/evaluations/`. O contêiner tem limite de 0,75 CPU, 512 MiB de RAM,
rede desativada e pausa térmica em 80 °C com retomada em 70 °C.

## Verificação da implantação

O primeiro v3 concluiu o lote Worker 70/70 e as baterias rápida e ampla; ambas
reprovaram o candidato por métricas de combate. O piloto revelou que o estado
de investigação não era atribuído quando o cérebro neural perseguia a posição
memorizada. O v3 foi parado sem apagar checkpoints, e a correção passou nos
testes locais de combate, currículo 22 e avaliador determinístico. Dentro da
imagem r2, o teste do currículo 22 passou e o candidato Worker antigo registrou
210 quadros de investigação do observador e 60 do aliado, sem vazamentos.

O serviço r2 completou o primeiro Worker na geração 70, gravou relatórios
rápido e amplo e passou automaticamente a treinar o Guard. O candidato Worker
foi reprovado (rápido: score 0,850, cinco regressões; amplo: score 0,988, duas
regressões), portanto não foi promovido. Após esse ciclo, o contêiner ficou
saudável com `restart=always`; os dois serviços anteriores ficaram parados com
`restart=no`. Os seis testes CTest afetados e os 17 testes Python passaram.

## Revisão de autoria do fogo amigo (protocolo 4)

O avaliador `--team-attributed` acrescenta nove colunas ao CSV: acertos,
mortes e dano em aliados atribuídos ao atirador principal, aliado ou terceiro.
O modo `--team` e os relatórios preservados do serviço v3-r2 continuam no
protocolo 2 de equipe. O gate novo exige somas consistentes, barra morte causada pelo papel
candidato e piora no dano ou nas mortes globais; uma morte igual, causada por
outro papel nas duas políticas, não reprova automaticamente o candidato.

No encontro `firing_lane` da semente 152239, o Guard causou os 2 acertos,
10 de dano e 1 morte em aliado tanto no ativo quanto com Worker 280. O Worker
não causou fogo amigo. Repetindo as quatro sementes (152201, 152213, 152227,
152239) e quatro modos por política com o mesmo binário local, o Worker 280
obteve score combinado 1,114778 e nenhuma regressão sob o gate atribuído.
O relatório local está em
`build/ai-perception-v3-r2/worker280-wide/attributed-v3/worker280-report.json`.
Essa elegibilidade é de desenvolvimento: ainda exige sementes inéditas e
playtest. Nenhum `assets/ai/*.brain` foi substituído.

O `train_service.py` aceita o protocolo 4 em diretório de dados isolado e
currículo 22, usando `--team-attributed` na avaliação. A imagem
`gdp-training:perception-v4-20261019` foi construída no Zima a partir da
fonte congelada r2, com mudanças apenas em `tools/ai_gameplay_eval.cpp`,
`tools/check_ai_gameplay_eval.cmake` e `SOURCE_COMMIT`; o executável de treino
`gdp` manteve o mesmo SHA-256. O teste determinístico passou no servidor.

O serviço v3-r2 foi parado com `restart=no`, sem apagar seus dados. O serviço
`gdp-perception-v4` usa `/DATA/AppData/gdp-perception-v4/data` e retomou a
população completa mais recente de cada papel: Worker 980, Guard 980,
Officer 980, Engineer 1120 e Robot 1190. O estado conserva a rotação e os
13 lotes concluídos por papel antes da migração. Um piloto na própria imagem
reproduziu a autoria Guard/Worker da semente 152239. A reavaliação ampla do
Worker 280 na imagem v4 obteve score 1,114778, sem regressões; o candidato
foi arquivado em `data/candidates/` sob o gate v4, sem substituir os cérebros
ativos. O serviço novo ficou saudável e iniciou Worker a partir da população
980. O primeiro lote chegou ao Worker 1050; seu relatório rápido usou
`paired-solo-team-perception-attributed-v4` e o reprovou por três regressões
de dano ao jogador em mapas/equipe, sem reprovação espúria por fogo amigo.
Após esse lote, o serviço passou ao Guard, ficou saudável e recebeu
`restart=always`. Os dados antigos e novos permanecem separados.

## Correção da avaliação de investigação (v4-r2)

Um Guard candidato produziu 28 quadros de visão do observador, abaixo dos
30 exigidos pelo fixture. O avaliador v4 tratava esse comportamento fraco
como erro fatal de dados e reiniciava o serviço sem concluir o lote. A revisão
v4-r2 registra a violação como regressão `investigation_contract` do candidato;
um fixture ativo inválido continua sendo erro. O mesmo relatório Guard foi
reprocessado com duas regressões de contrato, sem exceção. Os 21 testes Python
passaram.

A imagem `gdp-training:perception-v4-r2-20261019` altera apenas a classificação
da avaliação Python e a identificação da fonte. O executável de treino e o
avaliador C++ mantêm os hashes da imagem v4. O serviço anterior foi parado com
`restart=no`; os dados continuam em `/DATA/AppData/gdp-perception-v4/data`.
O serviço `gdp-perception-v4-r2` usa dados separados, retomou Worker 1050,
Guard 1890, Officer 980, Engineer 1120 e Robot 1190, preservando a rotação.
O primeiro lote Guard chegou à geração 1960; o relatório rápido o reprovou
com score 0,465 e 17 regressões, incluindo a percepção insuficiente. O serviço
prosseguiu ao Officer, ficou saudável e recebeu `restart=always`.

O Worker 280 foi novamente avaliado na imagem v4-r2: score 1,114778, nenhuma
regressão, arquivado apenas como candidato. Para playtest local, há pacotes
isolados em `build/ai-playtest-2026-10-19/`: Worker aprovado com outros quatro
ativos, e comparativos experimentais individuais de Officer 70, Engineer 280
e Robot 70. Esses três reprovaram na avaliação ampla v4-r2; Guard permanece
ativo em todos os pacotes.

# Plano de treino dos cérebros inimigos — 2026-10-01

## Ponto de partida

O treino atual usa o contrato tático `GDP_ENEMY_BRAIN 3` (12 sensores, 12
neurônios ocultos, 5 ações), `reward_rules=5`, currículo 13 com quatro
repetições por estágio e `runtime_control_all=1`. Os cinco arquivos ativos em
`assets/ai/` continuam no contrato legado v2, geração 200. Checkpoints novos
ficam em `build/ai-runs/`; nenhum deles entra no jogo distribuído apenas por
ter um fitness ou holdout maior.

A rodada de 2026-10-01 (seed 42, população 8, 16 gerações por tipo) comparou os
campeões finais com os assets ativos. Worker, Guard, Engineer e Robot regrediram
em combate nos mapas. O Officer da geração 115 ficou disponível somente para
playtest em `build/ai-test/officer-v5-20261001/`; melhorou a mobilidade, mas
perdeu pressão na cobertura central e contra Bhop. A última rodada chegou a
92 °C. Na abertura deste plano, o sensor `coretemp` mostrou 93–96 °C, com
limiar `high` de 84 °C e `crit` de 100 °C. Não iniciar uma nova rodada longa
nesse estado.

## 1. Congelar a comparação

- Guardar a revisão do código, o contrato, as regras de recompensa, o currículo,
  a seed, a população, o número de gerações e os hashes dos cinco assets ativos
  junto de cada relatório. Comparar políticas sempre no mesmo binário.
- Usar os checkpoints já existentes para montar uma tabela por arquétipo e
  cenário: `open`, `center_cover`, `elevated`, `group`, `boss_cover` para Robot,
  além de `ground_strafe` e `bhop` em duelo. Registrar dano, acertos, tiros,
  recargas, deslocamento, ticks neurais e retorno à IA roteirizada.
- Separar as seeds usadas para escolher um candidato das seeds finais de
  avaliação. Não escolher o melhor checkpoint usando o mesmo holdout que será
  apresentado como prova de generalização.
- Preservar os arquivos e logs da bateria de 2026-10-01 como baseline. Não
  comparar diretamente fitness de `reward_rules` ou currículos diferentes.

Primeiro registro concluído: `build/ai-plan-baseline-20261001.json` contém a
revisão Git e os hashes dos cinco assets ativos, cinco checkpoints finais e
respectivos logs. É um manifesto de entradas; nenhuma avaliação de gameplay
nova foi executada ao gerá-lo.

## 2. Investigar a diferença entre treino e gameplay

- Reproduzir os fracassos mais claros: Worker sem dano nos mapas, Guard contra
  Bhop, Robot em `boss_cover` e Engineer com pouco deslocamento. Medir em que
  momento o inimigo perde visão, fica bloqueado, recarrega, tenta atacar sem
  acerto ou entra no fallback roteirizado.
- Ampliar o diagnóstico do estágio `RealMaps`: pelo menos cobertura central,
  corredor conectado, entrada/saída de porta e Player em movimento físico.
  Esses casos devem usar as mesmas colisões e regras de projétil do jogo.
- Antes de alterar pesos da recompensa, verificar se a política tem sensores e
  oportunidades reais para resolver cada caso. Recompensar apenas resultados
  observáveis, sem pontos por emitir uma direção ou pedido de ataque.

## 3. Fazer um experimento pequeno por vez

1. Alterar primeiro a variedade e a frequência dos cenários de mapa real no
   treino, preservando contrato, sensores e assets ativos. Rotular o currículo
   novo; manter seeds fixas de desenvolvimento e um conjunto final intocado.
2. Rodar um arquétipo por vez em diretório novo, começando pelo fracasso que a
   mudança pretende corrigir. Uma rodada curta serve para verificar métricas e
   estabilidade; várias gerações ou fitness alto não autorizam promoção.
3. Se o comportamento continuar ruim, ajustar um termo da recompensa por vez,
   com teste de direção e limite do termo. Registrar a versão das regras e
   repetir a comparação pareada com a baseline.
4. Reavaliar os cinco arquétipos após uma alteração compartilhada. Officer e
   Engineer dependem também de suporte automático; esse suporte não deve
   receber crédito de ataque neural.

## 4. Gate de candidato

Um candidato só segue para playtest quando passar por: (a) carregamento e
testes determinísticos; (b) bateria pareada nas mesmas seeds, mapas e durações;
(c) ausência de regressão relevante em cobertura, Bhop, navegação, fallback e
letalidade do boss; (d) inspeção dos piores casos, não apenas da média. O
playtest visual confirma orientação, leitura do combate e equilíbrio da
campanha antes de qualquer substituição em `assets/ai/`. Manter o ativo para
rollback.

A ferramenta local `tools/ai_auto_promote.py`, na abertura deste plano, ainda
não estava versionada e, sem `--dry-run`, substituía um asset ativo após o gate
numérico. Durante este plano,
usá-la apenas para análise com `--dry-run`. Antes de integrá-la ao fluxo,
alterar o padrão para gerar relatório e exigir uma etapa explícita de promoção
depois do playtest visual. Não executar `train` ou `promote` nessa ferramenta
com o padrão atual.

## 5. Limite de execução nesta máquina

Executar avaliações e treinos pesados somente com temperatura estabilizada
abaixo do limiar `high` informado pelo sensor. Rechecar durante a rodada e
interromper se a temperatura voltar a subir até esse limiar. `--cooldown-ms`
pausa entre gerações, mas não substitui esse monitoramento. Não rodar os cinco
treinos em paralelo. Até a máquina esfriar, fazer apenas análise de código,
logs e relatórios já produzidos.

## 6. Execução contínua no ZimaOS

Implantação iniciada em 2026-10-01, em `/DATA/AppData/gdp-training`, com o
runtime `gdp-training:d6689dcefcd6`. O serviço alterna os cinco arquétipos,
população 8, lotes de 70 gerações e avaliações pareadas ao fim de cada lote.
Limites: 0,75 CPU, 512 MiB de RAM; pausa térmica em 80 °C e retomada em 70 °C.
Os checkpoints e relatórios ficam no armazenamento persistente do servidor.

O teste de implantação concluiu lotes dos cinco arquétipos e validou a retomada
do Guard da geração 7 para a 8. O reinício usa o campeão e cria uma nova
população. O serviço não substitui assets do jogo; a avaliação final e o
playtest continuam sendo etapas separadas. A implantação inicial não comprova
estabilidade por 24 horas. Operação e reprodução: `tools/zimaos/README.md`.

## Evidência principal

- `05_enemies/ai_training_review.md`, seção “Bateria de treino e gate de gameplay”.
- `src/game/ai/WorkerTraining.cpp` e `src/game/ai/TrainingReward.cpp`.
- `tools/ai_gameplay_eval.cpp` e `tools/ai_auto_promote.py` (arquivo local).
- `build/ai-runs/tactical-v5-20261001-*.log` (artefatos locais, ignorados pelo Git).

## 7. Atualização do serviço: currículo 15 e população completa

Implementada em 2026-10-01, imagem `gdp-training:training-v2-dc5f9444d733`.
Cada geração publica a população ranqueada e seus parâmetros; o serviço usa
`--resume-population` depois da importação inicial do campeão antigo. O teste
C++ comprovou igualdade de todos os indivíduos, pesos, fitness e holdout entre
execução contínua e execução dividida. A retomada real do Officer usou a
população da geração 1058 para começar na 1059.

O currículo 15 amplia os encontros de mapa real: cobertura por ambos os lados,
entrada/saída da visão, altura baixa acessível a melee e fases do Robot. As
recompensas continuam em `reward_rules=5`; não comparar fitness bruto com a
rodada do currículo 13. A bateria ampla usa quatro seeds de desenvolvimento e
1200 frames no primeiro lote de cada tipo e depois a cada cinco lotes. O
arquivo de candidatos exige ausência de regressão relevante por cenário e
seed, mantendo os melhores dentro de cada protocolo. Seeds finais separadas
estão configuradas para avaliação explícita de 1800 frames, fora da seleção.

Piloto concluído para Guard, mesmos pesos iniciais e duas seeds, sete gerações
por braço: população 8 levou em média 16,02 s de treino; população 16, 23,02 s.
Dano médio nos mapas: 99,875 versus 99,75; duelos: 15 versus 14. Ambos os braços
falharam no critério por causa de regressão em Bhop. O piloto não mostrou
vantagem consistente de 16; o serviço permanece em 8. Isso não substitui um
experimento mais longo. Relatório local: `build/ai-upgrade-v2/population-comparison.json`.

O primeiro lote contínuo novo do Officer concluiu e gerou a bateria ampla;
foi rejeitado para seleção por quatro regressões por seed/cenário, apesar de
score médio acima da baseline. Os cérebros ativos permanecem preservados. Na
reinicialização observada do ZimaOS, `unless-stopped` deixou o contêiner parado;
a configuração foi ajustada para `always`. Operação e parada permanente estão
em `tools/zimaos/README.md`. Registro da implantação:
`/DATA/AppData/gdp-training/deployment-training-v2-2026-10-01.json`.

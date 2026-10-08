# Avaliação dos cérebros treinados — 02/10/2026

## Conclusão

Os cinco campeões mais recentes apresentam regressões em comparação aos
cérebros ativos. Apenas o Worker arquivado da geração 1120 passou pelo critério
experimental de avaliação final. Ele é candidato a playtest; os resultados
não autorizam concluir que há melhora geral do jogo.

O arquivo de melhores candidatos cumpriu sua função: preservou um Worker
melhor que o campeão mais recente. O número de gerações não é um critério
suficiente para escolher o cérebro que será distribuído.

## Método

- Execução no ZimaOS, com a imagem imutável
  `gdp-training:training-v2-dc5f9444d733`.
- Comparação pareada contra os assets ativos, todos na geração 200, usando
  o mesmo binário e as mesmas condições por cenário/seed.
- Cinco campeões mais recentes e o melhor Worker previamente escolhido pela
  bateria ampla de desenvolvimento. Candidatos congelados antes dos testes,
  com SHA-256 registrado; resultados finais não alteraram o ranking.
- Oito seeds reservadas: 10007, 11003, 13001, 17011, 19001, 23003, 29009 e 31013.
- Até 1800 frames, equivalentes a 30 segundos simulados por encontro.
- 192 execuções do avaliador, 296 pares de cenário/seed, 592 amostras neurais
  dos arquétipos avaliados. Todas tiveram controle neural efetivo.
- Critério existente: score de dano normalizado com peso de 60% para mapas e
  40% para duelos; regressão por seed acima do máximo entre 10% do dano ativo
  e meio acerto do tipo impede aprovação, mesmo com score médio melhor.

## Resultados

Dano médio causado ao jogador: **ativo → treinado**. As comparações são entre
os dois cérebros do mesmo tipo; médias de tipos diferentes não são um ranking.

| Candidato | Geração | Mapas | Duelos | Pares com regressão | Resultado |
|---|---:|---:|---:|---:|---|
| Worker arquivado | 1120 | 75,00 → 75,00 | 4,38 → 5,00 | 0 | Passou; candidato a playtest |
| Worker recente | 3570 | 75,00 → 61,25 | 4,38 → 5,00 | 8 | Reprovado |
| Guard recente | 3570 | 96,88 → 94,88 | 15,50 → 11,50 | 10 | Reprovado |
| Officer recente | 3648 | 100,00 → 93,94 | 6,00 → 7,50 | 2 | Reprovado |
| Engineer recente | 3570 | 73,28 → 71,09 | 7,19 → 8,75 | 10 | Reprovado |
| Robot recente | 3574 | 93,60 → 70,10 | 73,13 → 24,63 | 40 | Reprovado |

### Worker

O arquivado mantém 24 vitórias em 32 encontros de mapas. Nesses 24 encontros
vencidos pelos dois cérebros, o tempo médio para derrotar o jogador cai de
12,16 para 10,96 segundos. O ganho de dano em duelos é pequeno: 0,625 ponto
em média; não houve vitória do inimigo nos duelos.

O recente falha nas oito seeds de campo aberto: dano médio desse cenário cai
de 100 para 45, sem derrotar o jogador em nenhuma delas. Também demora mais
para vencer em cobertura e em grupo.

### Guard

O dano contra o jogador pulando cai de 31 para 19 em média. Há regressões
acima da tolerância em sete das oito seeds desse duelo. Também ocorrem perdas
em cobertura e em uma seed de campo aberto. Maior deslocamento não demonstra
melhor combate.

### Officer

Há melhora de dano nos duelos, mas duas falhas graves em cobertura:
seed 23003, 100 → 6; seed 29009, 100 → 0. O score médio de 1,0314 esconderia
essas falhas se a aprovação ignorasse os pares individuais.

### Engineer

Há melhora nos duelos e na média de cobertura, mas o cenário em altura cai
de 100 para 70,625 de dano médio. Sete das oito seeds desse cenário apresentam
regressão acima da tolerância. Vitórias nos mapas caem de 16/32 para 9/32.

### Robot

É o caso mais fraco desta rodada: dano médio nos duelos cai aproximadamente
66,3%. Há regressão nos 16 duelos e nas 24 amostras de campo aberto, cobertura
central e altura. Vitórias nos mapas caem de 32/40 para 12/40.

Há um ganho localizado em `boss_cover`, na fase de pouca vida: dano médio
68 → 90, com vitórias 0/8 → 4/8. Esse ganho não compensa as perdas restantes.

## O que os resultados indicam para o treino

O histórico de desenvolvimento contém oito baterias amplas por tipo. Worker
passou em seis; os demais não passaram em nenhuma. O comportamento oscila
entre lotes, inclusive com quedas grandes no Robot. Isso justifica investigar
a relação entre fitness de treinamento e desempenho real dos encontros.
Esta avaliação não identifica sozinha a causa das regressões.

Recomendações para a próxima alteração:

1. Preservar o Worker 1120 como referência e realizar playtest antes de
   qualquer promoção. Manter os cérebros ativos nesta rodada.
2. Revisar o treino e a seleção de pais com foco nos encontros que falharam:
   campo aberto para Worker, Bhop para Guard, cobertura para Officer e
   cobertura/altura/duelo para Engineer e Robot. A seleção de geração atual
   usa fitness dos estágios; a bateria externa arquiva candidatos sem
   participar dessa seleção de pais.
3. Acrescentar avaliação específica de suporte em grupos mistos para Officer
   e Engineer. As fixtures atuais usam três inimigos do mesmo tipo, sem dano
   nos aliados; os resultados de dano não medem adequadamente comandos e
   reparos em equipe.
4. Reservar novas seeds para uma próxima confirmação final. As oito seeds
   deste relatório agora foram consultadas e, se orientarem ajustes, deixam
   de ser uma confirmação independente dessas alterações.

## Limites e operação

Mapas usam jogador que se move ou permanece na cobertura, sem atirar.
Duelos usam jogador automatizado, mira direta e uma pistola de avaliação
mais fraca que a da campanha. Todos os inimigos, ativos e candidatos,
perderam os 16 duelos de cada avaliação; dano maior não significa vitória.
Não houve campanha completa nem playtest humano.

O dano em mapas é limitado a 100. Empates no teto exigem considerar tempo
para vencer. A plataforma alta do cenário `elevated` é diferente da altura
baixa acessível do currículo; Worker sem dano ali não prova falha de treino.

O treino foi pausado somente durante a avaliação e retomou a população
completa do Robot na geração 3575. Verificação posterior: contêiner saudável,
política de reinício `always`, proteção térmica ativa e temperatura de 52 °C.
Nenhum cérebro foi promovido ao jogo; nenhuma configuração de treino foi
alterada nesta avaliação.

## Evidências

- Resumo detalhado: `build/ai-evaluation-20261002/relatorio.md`.
- Métricas, manifesto e hashes: `build/ai-evaluation-20261002/results.json`.
- Amostras filtradas: `build/ai-evaluation-20261002/samples.csv`.
- CSVs brutos e cópias dos cérebros congelados:
  `build/ai-evaluation-20261002/server/data/`.
- Servidor: `/DATA/AppData/gdp-training/data/evaluation-audits/20261002-heldout/`.

Revisão do runtime:
`aa93bb4bfbc18a0957a1871f07891ae4a3679b2c+training-v2-dc5f9444d733`.

ID da imagem:
`sha256:0e89c8a38f9c532da1644cce340b781cb95dff6afa19de216c2af4c89d450ab5`.

# Avaliação das notas dos cérebros — 06/10/2026

## Resultado

O Worker da geração 26460 foi o único dos cinco campeões recentes a passar
pela avaliação independente. Seu ganho é pequeno e concentrado no duelo
contra o jogador pulando. Guard, Officer, Engineer e Robot tiveram regressões
acima da tolerância em cenários individuais.

| Tipo | Geração | Fitness do treino | Score independente | Regressões | Parecer |
|---|---:|---:|---:|---:|---|
| Worker | 26460 | −20,031 | 1,057 | 0 | Passou; candidato a playtest |
| Guard | 26460 | 1,015 | 0,876 | 11 | Reprovado |
| Officer | 26538 | 0,681 | 0,832 | 25 | Reprovado |
| Engineer | 26402 | 0,101 | 1,067 | 9 | Reprovado |
| Robot | 26505 | 1,454 | 0,864 | 20 | Reprovado |

O score independente usa **1,0 como paridade com o cérebro ativo**. Um score
acima de 1,0 só passa se nenhum par cenário/seed piorar além da tolerância:
o maior valor entre 10% do dano ativo e meio acerto do arquétipo. O Engineer
mostra por que essa segunda condição importa.

O fitness do treino tem escala e seeds próprias de cada geração. Ele não é
uma nota de 0 a 10 nem um ranking entre arquétipos. No Worker, −20,031 resulta
da penalidade `combat_competent=0`: apenas três episódios de combate com acerto
onde o treino exigia pelo menos quatro. Mesmo assim, o Worker passou no teste
externo. O Robot teve o maior fitness interno e falhou no combate independente.
Os dois casos mostram que o fitness atual não prevê de forma confiável a
aprovação externa.

## Dano ao jogador: ativo → candidato

| Tipo | Mapas | Duelos | Vitórias nos mapas, ativo → candidato |
|---|---:|---:|---:|
| Worker | 75,00 → 75,00 | 3,75 → 5,00 | 24/32 → 24/32 |
| Guard | 97,25 → 91,13 | 16,50 → 12,00 | 28/32 → 27/32 |
| Officer | 100,00 → 73,75 | 7,13 → 6,75 | 32/32 → 8/32 |
| Engineer | 72,19 → 87,81 | 8,13 → 6,56 | 16/32 → 24/32 |
| Robot | 96,00 → 88,40 | 68,13 → 52,13 | 32/40 → 29/40 |

- **Worker:** os mapas empatam no teto de dano em três cenários e ficam em zero
  para ambos no cenário alto. No duelo `bhop`, dano médio 7,5 → 10. Nenhuma
  das 16 partidas de duelo terminou com vitória do inimigo.
- **Guard:** perde em cobertura central, 89 → 64,5, e no duelo `bhop`, 33 → 23.
  Foram cinco e seis regressões por seed, respectivamente.
- **Officer:** as perdas ocorrem em cobertura central, altura, campo aberto e
  `bhop`. O dano em mapas cai de 100 para 73,75 em média.
- **Engineer:** melhora em cobertura central e campo aberto, mas há quatro
  perdas individuais em cobertura e cinco no duelo `bhop`. A média ponderada
  1,067 esconde essas perdas.
- **Robot:** perde em campo aberto, 100 → 65, e no duelo em solo, 72,8 → 41,5.
  Os oito pares do duelo em solo ultrapassam a tolerância de regressão.

## Método e limites

- Cérebros mais recentes disponíveis congelados em 06/10/2026 às 12:01 UTC,
  antes de consultar as novas seeds. Arquivos e hashes constam no manifesto.
- Oito seeds inéditas: 61001, 63017, 67003, 71011, 73009, 79001, 83003,
  89009; até 1800 frames por encontro, equivalentes a 30 segundos simulados.
- Comparação pareada com o cérebro ativo da geração 200, usando o mesmo binário
  e as mesmas condições por seed. Foram 160 execuções, 248 pares de
  cenário/seed e 496 amostras neurais verificadas.
- Os mapas usam jogador que se move ou permanece na cobertura sem atirar.
  Os duelos usam jogador automatizado com mira direta e pistola de avaliação.
  Todas as partidas de duelo terminaram com vitória do jogador, para os
  cérebros ativos e os candidatos. Dano adicional não demonstra vitória.
- Dano em mapas é limitado a 100. Um empate nesse teto exige examinar tempo
  para vencer. Os testes não substituem uma campanha completa ou playtest.
- O teste de 04/10/2026 usou outro conjunto inédito de seeds e os campeões
  disponíveis naquele dia. Seus cinco candidatos falharam; as gerações e
  condições não permitem comparar diretamente os scores entre as datas.

## Decisão sobre as notas

O Worker 26460 merece playtest controlado. Guard, Officer, Engineer e Robot
precisam de ajuste e nova validação antes de qualquer promoção. A próxima
revisão do treino deve verificar por que o fitness premia políticas que
regrediram nos testes externos, em especial o Robot, e por que penalizou o
Worker aprovado. As seeds deste relatório agora foram consultadas; alterações
orientadas por elas precisam de outro conjunto reservado para confirmação.

O treino foi retomado após a bateria. A verificação posterior encontrou o
contêiner saudável e treinando Engineer, com CPU a 51 °C. Os assets ativos
do jogo não foram alterados.

## Evidências

- Manifesto, SHA-256 e relatórios completos:
  `build/ai-evaluation-20261006/results.json`.
- CSVs brutos e cópias congeladas:
  `build/ai-evaluation-20261006/server/data/evaluation-audits/20261006-latest/`.
- Amostras filtradas: `build/ai-evaluation-20261006/samples.csv`.
- Fitness congelado: `build/ai-evaluation-20261006/fitness-snapshot.json`.
- Notas de desenvolvimento no momento da análise:
  `build/ai-evaluation-20261006/development-snapshot.json`.
- Servidor: `/DATA/AppData/gdp-training/data/evaluation-audits/20261006-latest/`.

Imagem usada: `gdp-training:training-v2-dc5f9444d733`.
ID: `sha256:0e89c8a38f9c532da1644cce340b781cb95dff6afa19de216c2af4c89d450ab5`.

# Piloto de treino cooperativo e seleção Officer — 26/10/2026

## Decisão

O currículo experimental 23 foi implementado como opção explícita
`--combat-pressure-curriculum` para Guard e Engineer. Nenhum dos cérebros
produzidos passou na bateria ampla. O serviço `gdp-perception-v4-r3` no ZimaOS
continua usando o currículo 22 e permanece isolado do piloto. Worker 1820 e
Robot 2030 continuam ativos; Guard, Officer e Engineer mantêm seus cérebros
anteriores. Nenhum asset foi promovido e o Download não foi alterado.

## Protocolo A/B local

Cada braço começou do zero com a mesma seed, população 8, 70 gerações,
sete gerações de aquecimento e intervalo de 1 ms. As seeds de treino foram
20261026 e 20261027. O v22 e o v23 usaram o mesmo binário em cada rodada;
os cérebros ativos foram os mesmos cinco. O avaliador
`gdp_ai_gameplay_eval` teve SHA-256
`dc416746a2ec64c1f60ccfcdfd8c2d70bf23eaee24adbbb940df87bf4fbed000`.
Cada candidato passou por mapas, duelo, equipe com autoria de fogo amigo e
investigação. A triagem rápida usou seeds 152201/152213 e 480 quadros; a ampla
acrescentou 152227/152239 e usou 1200 quadros. As oito seeds finais ficaram
reservadas.

Os arquivos locais ignorados pelo Git ficam em
`build/ai-combat-pilot-2026-10-26/` (primeira regra v23),
`build/ai-combat-pilot-v23b-2026-10-26/` (bônus condicionado a não acertar
aliados) e `build/ai-combat-pilot-v23c-2026-10-26/` (penalidade mais forte para
Guard e métrica de dano do aliado no Engineer). Cada pasta contém
`summary.json`, logs, populações, hashes e CSVs pareados. O teste selecionou
melhorias por papel a partir dessas três iterações; o histórico completo
continua nessas pastas.

| Papel e regra | Score amplo, seeds de treino 20261026 / 20261027 | Regressões amplas | Acertos no holdout | Dano aliado no holdout |
| --- | --- | --- | --- | --- |
| Guard v22 | 0,448 / 0,501 | 31 / 32 | 0 / 0 | 0 / 0 |
| Guard v23, primeira regra | 0,713 / 0,929 | 18 / 22 | 13 / 10 | 96 / 40 |
| Guard v23, regra final do piloto | 0,635 / 0,753 | 27 / 20 | 3 / 10 | 24 / 16 |
| Engineer v22 | 0,953 / 1,038 | 11 / 13 | 22 / 49 | 0 / 0 |
| Engineer v23, segunda regra | 0,950 / 1,032 | 11 / 4 | 62 / 52 | 5 / 0 |
| Engineer v23, crédito de dano aliado | 0,589 / 0,555 | 17 / 16 | 53 / 47 | 0 / 10 |

O treino v23 reduziu o problema do Guard sem disparos, mas ainda escolhe
comportamentos com dano a aliados. O bônus por dano do Guard aliado, mesmo
condicionado ao ataque neural do Engineer, piorou as duas avaliações amplas;
por isso não entra na regra final do piloto. A métrica de dano aliado foi
mantida apenas para diagnóstico. Isso não autoriza treino permanente v23 no
ZimaOS nem substituição de cérebros.

## Seleção Officer

As populações completas Officer 23800 e 25480 tiveram seus oito membros
comparados com os cérebros ativos congelados, no mesmo avaliador. Só os
campeões passaram na triagem rápida: 23800 marcou 1,105, sem regressões;
25480 marcou 1,132, sem regressões. Na ampla, nenhum dos 16 membros passou.
O 23800 marcou 1,082 com duas regressões (`center_cover` 66→42 e `bhop`
12→6); o 25480 marcou 1,088 com três (`open` 96→84 e `bhop` 12→6 em duas
seeds). Os relatórios, CSVs e hashes estão em
`build/ai-officer-selection-2026-10-25/`. Nenhum Officer chegou à bateria final.

## Gate de integração

O currículo 23 é opt-in e usa ID de população 23, separado do 22; os testes
focados verificam incompatibilidade de retomada, determinismo, atribuição de
dano aliado e recompensa. A suíte Release passou com 81/81 testes, e a suíte
Python passou com 51/51. Não houve candidato elegível na avaliação ampla,
então as oito seeds finais e o playtest de novos cérebros não foram executados.
A próxima mudança de objetivo precisa representar melhor a pressão do Player
sobre o aliado do Engineer e eliminar o dano aliado do Guard antes de um novo
serviço no ZimaOS.

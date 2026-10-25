# Diagnóstico do treino cooperativo — 25/10/2026

O serviço isolado `gdp-perception-v4-r3` segue saudável no ZimaOS, treinando
Guard, Officer e Engineer. Worker 1820 e Robot 2030 continuam ativos. Nenhum
cérebro adicional foi promovido nesta revisão.

## Avaliações do serviço

Na leitura dos relatórios rápidos já concluídos, cada papel tinha 17 lotes:

| Papel | Faixa do score | Elegíveis | Falha observada |
| --- | ---: | ---: | --- |
| Guard | 0,371–0,688 | 0 | 13–19 regressões por lote; dano insuficiente em mapas e equipe |
| Officer | 0,771–1,187 | 0 | o melhor lote ainda perde em ao menos uma combinação de cenário e seed |
| Engineer | 0,275–0,691 | 0 | 8–15 regressões por lote, sobretudo duelo e equipe |

O Guard 22190 terminou o holdout com zero acertos e oito recargas prematuras.
O treino selecionou esse cérebro apesar de zero acertos nos estágios de combate.
O piso de competência em `WorkerTraining.cpp` se aplica a Worker e Engineer,
mas não a Guard. O treino de percepção também não inclui os encontros de
`hard-case` usados em outro currículo; as opções são mutuamente exclusivas.

## Varredura de populações Officer

Foram copiadas, sem alterar o servidor, as populações completas das gerações
20370 e 21350. Os oito membros de cada uma foram comparados com os mesmos cinco
cérebros ativos, no mesmo executável local
`gdp_ai_gameplay_eval` (SHA-256
`dc416746a2ec64c1f60ccfcdfd8c2d70bf23eaee24adbbb940df87bf4fbed000`).
O código da imagem do servidor é
`ac4d85fcebd2f9942f84cf851b823b02577f8658`. As modalidades foram mapas,
duelo, equipe com autoria de fogo amigo e investigação.

| População | Membro | Rápida: 2 seeds, 480 quadros | Ampla: 4 seeds, 1200 quadros |
| --- | --- | --- | --- |
| Officer 20370 | 004, SHA-256 `abedc3a6ae596ec947fb0cda38f06a422fcd5cbfdd5225abc21d78cb9cf8572d` | 1,038; 0 regressões | 0,998; 4 regressões |
| Officer 21350 | 002, SHA-256 `e90db3d9c3326a48610361ac8b3ecbe524ac8b48472b675f29251cd7b9933315` | 1,177; 0 regressões | 1,097; 3 regressões |

Todos os outros membros das duas populações também falharam na bateria ampla.
Os sumários e CSVs locais estão em
`build/ai-officer-sweep-2026-10-25/` (artefatos ignorados pelo Git). A rápida
usa seeds 152201/152213; a ampla acrescenta 152227/152239. Não foi usada a
bateria final de oito seeds, reservada para candidatos que passem na ampla.

## Piloto isolado de Guard

Uma cópia da população íntegra Guard 22330 foi retomada por sete gerações no
controle e em um checkout separado com o piso de competência estendido a Guard.
Ambos usaram o currículo 22, população 8, seed preservada 20261019, os mesmos
cenários e `--cooldown-ms 1`. Na geração 22337, ambos terminaram com zero
acertos e oito recargas prematuras no holdout; os pesos do campeão eram
idênticos. O ajuste mudou apenas o valor registrado como fitness. Portanto,
esse piso, sozinho, não resolveu o problema e **não foi incorporado** ao jogo
nem ao serviço. Logs e cérebros do piloto estão em
`build/ai-guard-pilot-2026-10-25/` (ignorados pelo Git).

## Próximo experimento

O treino precisa oferecer sinal de combate antes da seleção final, sobretudo
para Guard e Engineer. Testar um currículo novo em dados separados, com controle
de mesma seed e avaliação rápida/ampla nas mesmas condições; versionar o
currículo e o formato de população se o objetivo de fitness mudar. Manter o
v4-r3 e seus dados intactos enquanto esse piloto é medido. Nenhum candidato
entra em `assets/ai/` ou no Download sem avaliação final e playtest.

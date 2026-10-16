# Candidatos isolados para playtest de IA — 15/10/2026

## Eleição

**Principal: pacote `selective`.** Mantém Worker e Guard ativos (legado v2) e
testa Officer r2 geração 2761, Engineer r2 geração 2750 e Robot do currículo
21 geração 7 (contrato cooperativo v4). É a composição mais promissora para
medir as novas mecânicas sem confundir o teste com as falhas já reproduzidas
no Guard r2. Não substitui os arquivos de `assets/ai/`.

**Diagnóstico: pacote `full-coop`.** Usa Worker r2 geração 2739, Guard r2
geração 2730, Officer r2 geração 2761, Engineer r2 geração 2750 e Robot 21
geração 7. É útil para observar coordenação em jogo e registrar as falhas de
Worker/Guard; ainda não é candidato à substituição integral.

Os dois pacotes estão em `build/ai-playtest-20261015/`, com cinco
`TYPE.brain` em cada diretório, manifestos e CSVs pareados. Os hashes exatos,
contratos, gerações e revisões de treino constam em `manifest.json`. O arquivo
portátil local é `build/ai-playtest-20261015.zip`, SHA-256
`64922cbca75e3cc46befa36c71473cdc1ea0aa753cfc102f6db571c92854b420`.
O Robot g7 foi escolhido em checkpoints interiores definidos antes da avaliação; sua
bateria individual ampla passou o gate anterior com score 1,207 e zero
regressões. Os demais r2 foram congelados do último lote completo disponível
na seleção anterior. Nenhum cérebro foi promovido.

## Evidência automática

Mesmo avaliador local para ativo e candidato: SHA-256
`bd64c6289f30af1748f53b56139998e839e692e821ae7c2924566934db199ac1`,
revisão fonte `dd079825b94629dca8d070435db4f6583443ca6e`. Seeds de
desenvolvimento 150001/150011/150031/150053, 1200 frames, mapas, duelo e
equipe. As sementes finais continuam reservadas. O score diagnóstico pondera
equipe 55%, mapas 30% e duelo 15%; não é um gate de promoção.

| Composição | Score | Equipe | Mapas | Duelo | Fogo amigo pior | Sem ação à distância |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Principal `selective` | 1,012 | 0,993 | 1,045 | 1,017 | 0 casos | 0 casos |
| Diagnóstico `full-coop` | 0,830 | 0,745 | 0,932 | 0,937 | 0 casos | 3 casos |

No pacote principal, o Boss causa 67,5 de dano médio no fixture com ajudantes,
contra 49,5 do ativo. Officer+Guard sobe de 35 para 42,5. Engineer+Guard cai
de 57 para 23,25; essa perda exige observação em jogo. O `full-coop` apresenta
três seeds de `center_cover` em que o Guard não dispara apesar de o ativo
causar dano. Sua queda nesses cenários impede recomendá-lo para substituição.

A telemetria nova atribui tiros por papel. No fixture `boss_helpers`, os
ajudantes Officer e Guard disparam **zero tiros** nas quatro seeds em ambos
os pacotes; só o Robot causa dano. Assim, esse fixture não valida a cooperação
dos ajudantes. A decisão automática dos dois pacotes é
`insufficient_evidence`, por essa limitação e, no `full-coop`, pelas três falhas
de ação. O playtest pode investigar o comportamento real, mas não remove a
necessidade de um fixture melhor, seeds finais inéditas e uma decisão de
promoção separada.

## Execução e observação

Na raiz do repositório, com o build local de Iron Seed 0.1.6. Em Linux,
`XDG_DATA_HOME` mantém configurações e saves do teste separados do perfil
normal:

```sh
XDG_DATA_HOME="$PWD/build/ai-playtest-20261015/profile-selective" build/release/gdp --test-brain-dir build/ai-playtest-20261015/selective
XDG_DATA_HOME="$PWD/build/ai-playtest-20261015/profile-full-coop" build/release/gdp --test-brain-dir build/ai-playtest-20261015/full-coop
```

O modo `--test-brain-dir` exige cinco arquivos válidos e carrega o contrato de
cada papel separadamente; o smoke headless ativou os cinco de cada pacote com
saída 0. Para comparar, jogar primeiro com os ativos sem a flag, depois com
`selective` e por último com `full-coop`. Registrar dificuldade, seed, sala,
papéis presentes, distância, saúde inicial, dano aliado, inimigos sem ataque
e se a pressão parece justa. Priorizar sala do Boss, salas grandes, cobertura
central e encontros Guard+Engineer. Testar em Easy, Normal e Hard.

O pacote em `build/` não é uma atualização pública. Não copiar esses arquivos
para `assets/ai/` ou para o Download. Após feedback de jogo,
ajustar o fixture de ajudantes e repetir a avaliação pareada/final antes de
considerar qualquer promoção.

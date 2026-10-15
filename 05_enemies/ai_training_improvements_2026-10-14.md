# Melhoria controlada do treino de IA — 14–15/10/2026

## Objetivo e separação

O treino r2 do ZimaOS continua sendo a referência experimental do protocolo 2.
Seus arquivos em `/DATA/AppData/gdp-coop-new-mechanics-r2` foram preservados.
Os cérebros de `assets/ai/` e o Download não foram alterados. O piloto novo usa
currículo 20, dados e imagem separados; nenhuma população 19 pode ser retomada
como 20, nem o inverso.

## Diagnóstico dos checkpoints anteriores

`tools/zimaos/checkpoint_sweep.py` verificou a revisão, os SHA-256 reais dos
executáveis e os pares `.brain`/`.population` completos antes de selecionar
gerações interiores dos lotes concluídos. O inventário ficou em
`build/ai-checkpoint-sweep/r2-inventory.json`. A avaliação usou o mesmo
executável r2 para ativo e candidato, seeds 150001/150011, 480 frames e os
mesmos cenários de mapas, duelo e equipe; os CSVs e relatórios estão em
`build/ai-checkpoint-sweep/evaluations/`. As seeds finais 151001...151099
permanecem reservadas.

| Papel | Geração | Score rápido | Regressões | Elegível |
| --- | ---: | ---: | ---: | --- |
| Guard | 70 | 0,588 | 19 | Não |
| Guard | 94 | 0,544 | 16 | Não |
| Guard | 117 | 0,572 | 17 | Não |
| Guard | 140 | 0,542 | 17 | Não |
| Guard | 187 | 0,480 | 16 | Não |
| Guard | 210 | 0,551 | 14 | Não |
| Robot | 47 | 1,388 | 7 | Não |
| Robot | 70 | 1,314 | 2 | Não |
| Robot | 94 | 1,563 | 4 | Não |
| Robot | 117 | 1,277 | 7 | Não |
| Robot | 140 | 1,082 | 10 | Não |
| Robot | 210 | 1,007 | 14 | Não |

Os relatórios antigos r2 foram copiados para `build/ai-ranking-audit/`; o
script `tools/zimaos/rank_checkpoints.py` recalcula cada relatório a partir dos
CSVs e recusa comparação se mudarem revisão, seeds, duração, protocolo ou
gerações dos cérebros ativos/aliados. Os relatórios antigos não registravam
o hash do binário; o inventário e os executáveis do contêiner r2 confirmaram
SHA-256 `15638bf7f721ef5d0820fe351f0a42422b59e1f8fe3f412dad75ed66a4a5d5b4`
para o avaliador. Nenhum checkpoint examinado passou; aumentar gerações sem
alterar o currículo não é evidência suficiente de progresso.

## Variante piloto

`--hard-case-curriculum` é opt-in e válido somente para Guard ou Robot com
`--cooperative-brain --all-scenarios --diverse-maps`. O currículo 20 acrescenta
dois episódios por geração com seeds derivadas de domínio próprio: Guard em
cobertura/grupo e mapa com jogador saltando; Robot contra salto com cobertura
e no encontro do Boss com ajudantes. A pontuação cooperativa já penaliza dano
aliado e retenção excessiva de fogo; os novos casos representam 25% da
pontuação combinada e penalizam zero acertos. O holdout padrão e currículo 19
continuam iguais. Os logs registram `hard_case_*` para diagnosticar o piloto.

O A/B usa a mesma revisão `8246d99e99c4a0a7f108ad24c148251d1e102cc9`,
imagem `sha256:23250d17635ba8231ddb99846b1841273ef8406a84643fffbae266ce3abd4cbd`,
população 8, 28 gerações, aquecimento de 7, seeds de treino 20261014 e
20261015 e as mesmas seeds de desenvolvimento acima. O script
`tools/zimaos/pilot_hard_cases.py` salva hash do binário, dos cérebros ativos,
do script e de cada campeão, além dos CSVs e relatórios, em
`/DATA/AppData/gdp-hardcase-pilot-8246d99/data/hard-case-pilot/`.

| Papel | Seed de treino | Currículo 19: score / regressões | Currículo 20: score / regressões |
| --- | ---: | ---: | ---: |
| Guard | 20261014 | 0,777 / 12 | 0,878 / 7 |
| Guard | 20261015 | 0,773 / 15 | 0,502 / 16 |
| Robot | 20261014 | 1,026 / 11 | 0,549 / 13 |
| Robot | 20261015 | 1,303 / 2 | 1,552 / 2 |

O ranking recalculado a partir dos CSVs está em
`build/ai-hardcase-pilot/results/{guard,robot}-ranking.json`; o pacote local
dos CSVs e logs tem SHA-256
`148996a884bc1e08d1092c94dbe00bc0cc1a49d105444034947598912c5968fa`.
**Nenhum dos oito candidatos é elegível.** O currículo 20 não ganhou nas duas
seeds de nenhum papel. No Robot, os campeões das duas seeds terminaram com
`hard_case_hit_episodes=0` e `hard_case_score=-2`; a penalidade de zero
acertos saturou e não forneceu uma comparação útil entre os candidatos que
falharam nesses episódios. Na primeira seed, o Robot só causou 3,6 de dano
médio nos mapas, com qualquer um dos currículos, indicando sensibilidade à
seed inicial. A variante 20 permanece **experimental e reprovada para uso no
serviço contínuo**. Não se justifica executar seeds finais ou playtest dela.

## Revisão experimental de 15/10

O currículo 21 tem flag própria, `--hard-case-curriculum-v2`, e populações
incompatíveis com 19 e 20. O caso extra do Robot no currículo 20 punha o
primeiro ajudante diretamente na linha do canhão. A revisão desloca esse
ajudante para o lado, aproxima o jogador no caso de salto e usa uma posição
válida próxima do centro da sala grande com o Boss em perseguição. A sala,
cobertura e dois ajudantes continuam presentes. A pontuação dos casos extras
deixa de truncar todo episódio sem acerto em −2; um tiro neural que ameaça o
jogador mas é desviado pode render até 1,05 ponto, desde que não haja acerto
nem dano aliado. Não atirar continua penalizado. O currículo 19, o 20 e o
holdout padrão não mudam na simulação.

Nos microtestes determinísticos, Guard (seed 20261014, população 8, geração 1)
causou 24 de dano em caso extra sem dano aliado; Robot (mesma seed, geração 6)
causou 18, também sem dano aliado. Entre gerações do Robot sem acerto, a
pontuação extra variou. Esses testes verificam a viabilidade dos cenários e a
retomada exata, não a qualidade dos cérebros em jogo.

A revisão fonte do piloto 21 é `68c8b98df2e63a118074c79fd4e770b41524ad72`;
a imagem isolada é
`sha256:7c613433a1034661aa6a74cfe42e2b8727094bc2fc2f77725607639135692e10`.
O executável de treino tem SHA-256
`ca9281e9c17bbb54c61ec2af2a389fdc6e35ca99c2d74308de9a33b46821e8da`;
o avaliador preserva
`15638bf7f721ef5d0820fe351f0a42422b59e1f8fe3f412dad75ed66a4a5d5b4`.
O A/B 19 versus 21 usa o mesmo binário por braço, duas seeds de treino,
população 8, 28 gerações, aquecimento 7, duas seeds de desenvolvimento e
480 frames em mapas, duelo e equipe. A comparação direta das pontuações
entre os pilotos 20 e 21 teria revisões diferentes; cada variante é julgada
contra seu controle 19 na própria revisão.

| Papel | Seed de treino | Currículo 19: score / regressões | Currículo 21: score / regressões |
| --- | ---: | ---: | ---: |
| Guard | 20261014 | 0,777 / 12 | 0,670 / 11 |
| Guard | 20261015 | 0,772 / 15 | 0,422 / 15 |
| Robot | 20261014 | 1,026 / 11 | 1,138 / 11 |
| Robot | 20261015 | 1,303 / 2 | **1,549 / 0** |

O A/B encerrou com código 0. O Guard piorou em score nas duas seeds. O Robot
ganhou score nas duas; seu campeão da seed 20261015 foi o único elegível na
bateria **rápida**. Esse cérebro, geração 28 e SHA-256
`5fd3f552a466d3f88b873ce2b79d944e96ebb5e672748626369a72aa3fb50489`,
acertou em um dos dois casos extras da geração final, causou 46 de dano ao
jogador e zero dano aliado. O ranking foi recalculado dos CSVs em
`build/ai-hardcase-pilot-v21/{guard,robot}-ranking.json`; o arquivo local do
piloto tem SHA-256
`b1bf74775be4339ece29a175633ecbaa2026f8e2fc0d3e32a9c95b6b6e007c3d`.

A bateria **ampla** desse único candidato rápido elegível usou seeds
150001/150011/150031/150053, 1200 frames e o mesmo avaliador. Ela deu score
1,192, mas **reprovou** por `boss_cover` na seed 150053: ativo causou 60 de
dano e o candidato zero. No CSV, o candidato não disparou e percorreu 25,277
unidades; nas outras três seeds desse cenário, causou 100 de dano. Essa é uma
falha de generalização em uma configuração de cobertura, não uma aprovação
parcial do gate. O relatório amplo recalculado está em
`build/ai-hardcase-pilot-v21/robot-wide-ranking.json`; o arquivo local dos CSVs
amplos tem SHA-256
`9b96fe42919b385d1c09aa8145d4674d8ddec1a84e7b11971e041d7a74208a03`.
Nenhuma seed final foi usada e nenhum cérebro foi promovido.

## Validação até agora

- Build Release local, 66/66 CTest; Python, 39/39 testes.
- Teste focal C++ dentro da nova imagem: passou com `/tmp` temporário.
- A primeira tentativa desse teste no contêiner somente leitura falhou por
  ausência de diretório temporário gravável; foi repetida com `tmpfs /tmp`.
- O serviço r2 foi pausado após completar o terceiro lote do Robot, com
  `restart=no` e estado/checkpoints preservados, para que o A/B tenha a CPU.
- O A/B concluiu com código de saída 0. O r2 voltou com `restart=always`,
  healthcheck saudável e retomou sua população completa do Worker na geração
  220. O piloto teve limite de 0,75 CPU, 512 MiB e pausa térmica 80/70 °C;
  durante a execução observada ficou perto de 50–57 °C.
- Na revisão 21, build Release local, 66/66 CTest e 39/39 Python passaram;
  o teste C++ focal passou também dentro da imagem isolada. O A/B de oito
  braços e a bateria ampla concluíram com código 0. A imagem 21 derivou da
  imagem anterior com dependências já presentes porque o ZimaOS não resolveu
  `deb.debian.org` ao tentar reconstruir a camada de pacotes; os novos
  executáveis e a revisão fonte foram copiados e tiveram hashes verificados.
  O r2 foi novamente retomado com `restart=always`, healthcheck saudável e
  população/checkpoints preservados, agora treinando Engineer.

## Critério de continuidade

O currículo 21 tornou os casos extras alcançáveis, mas o gate amplo revelou
uma configuração de cobertura na qual o Robot não atira. Antes de outro
treino longo, convém testar checkpoints intermediários previamente escolhidos
e ampliar a variação de cobertura/distância dos casos extras sem usar a seed
de desenvolvimento como fixture de treino. Um candidato que passar a bateria
ampla ainda precisa de seeds finais inéditas e playtest em Easy, Normal e
Hard, sobretudo na sala do Boss. Só uma decisão separada pode substituir
`assets/ai/*.brain` ou alterar o Download.

## Checkpoints interiores e pacote cooperativo — 15/10

Antes de observar os resultados, foram escolhidas as gerações 7, 14 e 21 do
Robot do currículo 21, seed de treino 20261015. O script
`tools/zimaos/evaluate_isolated_candidate.py` preservou os pares ativo/candidato
e a proveniência. A geração 7 (SHA-256
`e97b62b27f74fb85b402131309dc3e07fedab61acb8aabb814c63f5d84acc21a`)
passou o gate individual rápido (score 1,565, zero regressões) e amplo (1,207,
zero regressões). A geração 14 teve duas regressões rápidas e uma ampla; a 21
teve quatro regressões rápidas. Os CSVs e relatórios isolados foram copiados
para `build/ai-hardcase-pilot-v21/checkpoints-g7-g14-g21.tar.gz` (SHA-256
`b44ca605a003dc1efb3c27b02c3635247a8e2978702a3b39afeb9e24164cf5e2`).
Passar o gate individual da geração 7 não aprova um pacote de cinco cérebros.

Para medir a substituição coletiva, `tools/zimaos/evaluate_bundle.py` congelou
o último lote r2 completo de cada papel: Worker g2739
`f6310e1e7336f06bf5a5fec10a67d64bc01ab2375c61497fec474fe8d79e4a74`,
Guard g2730 `97f623e1be8e01a9cc2bdc817c7f292603e944c3ecbe486ba7dd014b7d910368`,
Officer g2761 `9220ea2c47961b7da3d3ada1ad90614c0c0965418c17d5beed8fc4158a42134b`,
Engineer g2750 `5254043b355c36f6b691acc2c1f4a58a0b038eda55116d6094aae9cc8d0de19c`
e Robot g2660 `ca526ac87442328248afe53902c8ce31872c886ef483ab3682c0fe3310d80080`.
Todos têm contrato cooperativo 3, regras 6, currículo 19 e população íntegra;
nenhum lote ainda em treino foi incluído. O avaliador r2 foi o mesmo nos dois
braços (SHA-256 `15638bf7f721ef5d0820fe351f0a42422b59e1f8fe3f412dad75ed66a4a5d5b4`).

`tools/zimaos/assess_bundle.py` é uma leitura diagnóstica, sem promoção. Seu
score provisório usa 55% equipe, 30% mapas e 15% duelo, com médias por
cenário/papel e suavização de meio acerto por papel. Segurança, falta de ação
e insuficiência do fixture aparecem em listas separadas; um score isolado não
decide a promoção. Foram usadas seeds de desenvolvimento 150001, 150011,
150031 e 150053 com 1200 frames, preservando as seeds finais.

| Pacote, bateria ampla | Score provisório | Equipe | Mapas | Duelo | Sem ação em confronto à distância |
| --- | ---: | ---: | ---: | ---: | ---: |
| Cinco r2 completos | 0,776 | 0,702 | 0,874 | 0,850 | 4 |
| Quatro r2 + Robot currículo 21 g7 | 0,830 | 0,745 | 0,932 | 0,937 | 3 |

O híbrido melhora o Robot: dano médio em mapas 94 ativo → 100 e em duelo
91,875 → 100; no Boss com ajudantes 49,5 → 67,5. Porém o Guard r2 cai de
98,25 → 63,25 nos mapas, de 16 → 8 no duelo e fica sem disparar em três seeds
de `center_cover` em que o ativo causa dano. O Worker r2 cai de 75 → 57,5 nos
mapas. No encontro Engineer+Guard, o pacote causa 18,75 contra 57 dos ativos.
Os novos cérebros não aumentaram dano aliado nas comparações pareadas desta
bateria, mas redução de fogo amigo não compensa automaticamente perda ampla de
pressão. No fixture `boss_helpers`, nenhum ajudante causa dano ao jogador em
qualquer braço, logo essa linha mede principalmente o Robot.

O pacote r2 amplo está em `build/ai-bundle-audit/r2-wide.tar.gz` (SHA-256
`868b54b1744571ef431b31ec0189571431be8d7dc8f950e27cee9db4bd460109`);
o híbrido amplo está em `build/ai-bundle-audit/hybrid-wide.tar.gz` (SHA-256
`d54c030de2a46846d043f34a2d2de2e56f3e54a3b9114acf4e8913c1272452c7`).
Os manifestos incluem seeds, duração, revisão e hashes dos cérebros/avaliador.
Os CSVs de equipe desses pacotes ainda usam o avaliador r2 de 35 colunas.
`tools/ai_gameplay_eval.cpp` ganhou nove colunas aditivas para atribuir tiros,
acertos e retenções de fogo por papel; novos experimentos terão de executar
ambos os braços com esse mesmo binário. O pacote permanece **experimental**;
nenhum cérebro ativo nem arquivo de Download foi alterado.

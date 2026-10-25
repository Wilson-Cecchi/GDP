# Retomada do treino cooperativo no ZimaOS — 25/10/2026

O contêiner `gdp-perception-v4-r2` estava parado desde 22/10/2026 após sair
com código 255. Seu `status.json` ainda dizia `running`, mas o Docker o mostrava
como `exited`. Ele permanece parado com `restart=no`, e todos os seus dados
continuam em `/DATA/AppData/gdp-perception-v4-r2/data`.

O novo serviço `gdp-perception-v4-r3` usa uma pasta e uma imagem separadas:

| Item | Valor |
| --- | --- |
| Fonte | `ac4d85fcebd2f9942f84cf851b823b02577f8658` |
| Imagem | `gdp-training:perception-v4-r3-20261025` |
| ID da imagem | `sha256:ba91ad7921d649345c974b11e3d5647b483f5dcb73500a83d1e82518a603dc2d` |
| Serviço | `/DATA/AppData/gdp-perception-v4-r3/compose.perception-v4-r3.yaml` |
| Dados | `/DATA/AppData/gdp-perception-v4-r3/data` |
| Protocolo | 4, currículo 22, população 8, lotes de 70 gerações |
| Rotação | Guard, Officer, Engineer |

A imagem contém Worker 1820 e Robot 2030 como cérebros ativos; Guard, Officer
e Engineer mantêm os ativos anteriores. Assim, os próximos candidatos são
comparados com os adversários que estão no jogo após a promoção. O código de
runtime também inclui a geometria de tiro compartilhada, o aviso de aliado
mais recente e a recuperação da IA quando não consegue disparar.

## Populações preservadas

As cinco populações completas do serviço anterior foram copiadas e validadas
sem alterar a origem. O serviço novo retoma apenas os três papéis pendentes;
Worker e Robot ficam arquivados para uma fase futura.

| Papel | Última geração íntegra | SHA-256 de `state.txt` |
| --- | ---: | --- |
| Worker | 20230 | `ce4e563d4520ddb6c1740484ac388bf254720c66f9f11f649cc21439ccc703fc` |
| Guard | 21140 | `66a8a2d3076a3019d1954cc627dae11dabe3116913bf6d8f4d075c969b659121` |
| Officer | 20160 | `e2e7e8fbe09f26030a2697e819193f10002377eb42fff44e517d11e98ae29631` |
| Engineer | 20300 | `18b9b47d3fe35087fcb7f753b9ad72ac8f0e78eda91c515528136b319dd66a9f` |
| Robot | 20370 | `71fb7e8589e8458fdf5c4dfe1ae6e174d59ed87f89b714bd922d46188247c6f8` |

O manifesto com os caminhos completos está em
`/DATA/AppData/gdp-perception-v4-r3/migration.json`. O primeiro lote novo
retomou `guard_generation_021140.population` e avançou até a geração 21145
durante a verificação inicial. Isso demonstra retomada real, sem reinício da
população. Ainda não representa um candidato aprovado.

## Validação e avaliação

- Compilação no ZimaOS concluída a partir da fonte acima; o teste C++
  `worker_training_curriculum_v22` e o avaliador determinístico passaram no
  ambiente de build. O contêiner carregou os cinco cérebros via
  `--test-brain-dir` e validou os três checkpoints retomáveis.
- A suíte Python local passou com 51 testes. `evaluate_final.py` agora usa
  mapas, duelos, equipe com autoria de fogo amigo e investigação para o
  protocolo 4. O teste novo confere as quatro modalidades e o gate v4.
- O contêiner novo iniciou com `restart=always`, healthcheck saudável e
  temperatura inicial de 45 °C. O antigo permanece `exited` e `restart=no`.

O treino continua produzindo candidatos isolados. Nenhum candidato futuro é
promovido automaticamente a `assets/ai/` ou ao Download.

## Acompanhar

```sh
ssh zima 'cd /DATA/AppData/gdp-perception-v4-r3 && docker compose -f compose.perception-v4-r3.yaml ps && cat data/status.json && tail -n 3 data/logs/guard/batch-b77xn3kf.log'
```

O log de um lote muda a cada rotação; depois dele, consulte o caminho `log` em
`data/status.json` para acompanhar o lote atual.

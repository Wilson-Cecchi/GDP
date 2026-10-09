# Treino cooperativo isolado no ZimaOS

O serviço cooperativo usa `compose.coop.yaml` em `/DATA/AppData/gdp-coop-training`.
Ele usa o código de jogo do commit `dcfbb0b66fd4240e156a8ad3d850700a959064d6`
e os scripts de serviço deste diretório. `SOURCE_COMMIT` registra o commit e os
prefixos SHA-256 desses scripts na imagem. Os arquivos de treino, avaliações e
candidatos ficam apenas em `/DATA/AppData/gdp-coop-training/data`.

O serviço tático anterior e seus dados continuam em `/DATA/AppData/gdp-training`.
Seu contêiner foi parado e recebeu `restart=no` para impedir que os dois
treinos disputem CPU depois de reiniciar o Docker. O contêiner cooperativo usa
`restart=always`, 0,75 CPU, 512 MiB de RAM, rede desativada, pausa térmica em
80 °C e retomada em 70 °C.

O currículo cooperativo começa uma população nova de oito membros. O formato
de cérebro é `GDP_ENEMY_BRAIN 4`, contrato 3; a população completa usa formato
2 e recompensa 6. Ele percorre Worker, Guard, Officer e Engineer. O Robot fica
fora deste ciclo porque o avaliador `--team` ainda não possui encontro com
Robot; não há base suficiente para selecionar um candidato cooperativo dele.

Cada lote tem 70 gerações. A avaliação rápida compara ativo e candidato no
mesmo binário, seeds 9001 e 9002, 480 frames; a ampla usa quatro seeds e 1200
frames a cada cinco lotes por tipo. Regressão de dano ao Player, aumento de
dano aliado ou qualquer morte aliada impedem a seleção. O serviço apenas
arquiva candidatos; `assets/ai/*.brain` não são substituídos. Antes de ativar
um candidato no jogo, ainda são necessários avaliação final e playtest.

Para acompanhar:

```sh
cd /DATA/AppData/gdp-coop-training
docker compose ps
docker compose logs --tail 20
cat data/status.json
cat data/state.json
```

Para voltar ao treino tático, pare primeiro o cooperativo e desative sua
reinicialização automática. Então reative a política e inicie o antigo:

```sh
cd /DATA/AppData/gdp-coop-training
docker update --restart=no gdp-coop-training
docker compose stop
cd /DATA/AppData/gdp-training
docker update --restart=always gdp-training
docker compose start
```

O teste curto inicial do Guard e seus CSVs estão em `data/smoke/`. Ele confirmou
o formato e o avaliador, mas foi rejeitado por dano aliado e pior dano ao
Player; não foi usado como ponto de partida do treino contínuo.

# Treinamento contínuo no ZimaOS

Servidor: `/DATA/AppData/gdp-training`. Runtime atual:
`gdp-training:training-v2-dc5f9444d733`, baseado no snapshot
`aa93bb4bfbc18a0957a1871f07891ae4a3679b2c+training-v2-dc5f9444d733`.
As mudanças de treino ainda estão no workspace, sem commit. O pacote de source,
seu SHA-256 e o identificador da imagem ficam registrados no servidor.

## Operação

No SSH do servidor:

```sh
cd /DATA/AppData/gdp-training
docker compose logs --tail 20
cat data/status.json
```

Para acompanhar eventos: `docker compose logs -f --tail 20`. A geração e as
métricas ficam no log indicado em `data/status.json`. Checkpoints: `data/runs/`;
comandos e revisão: `data/manifests/`; comparações: `data/evaluations/`;
candidatos selecionados: `data/candidates/`.

Para parar ou retomar:

```sh
docker compose stop
docker compose start
```

`restart: always` permite retorno após reinício do Docker/servidor, inclusive
se o desligamento do ZimaOS tiver parado os contêineres. Uma parada manual
vale até reiniciar o Docker. Para manter desativado, use
`docker update --restart=no gdp-training` antes de parar; um próximo
`docker compose up -d` reaplica a configuração. Dados ficam em `/DATA`.

## População completa e migração

- Um processo de treino por vez, alternando Worker, Guard, Officer, Engineer e
  Robot. Padrão: população 8, 70 gerações por lote, seed inicial 20261001,
  `--all-scenarios --diverse-maps`, pausa de 1000 ms entre gerações.
- Cada geração salva o campeão `.brain` e a população ranqueada em
  `TIPO_generation_N.population/`. Esse diretório é publicado com rename após
  todos os membros e metadados serem escritos e validados. Diretórios `.tmp`
  incompletos são ignorados.
- `--resume-population DIR` preserva pesos, IDs, ordem de seleção, seed,
  geração e progresso do currículo. Operações aleatórias derivam suas seeds
  da geração e do arquétipo; a continuação reproduz a execução sem interrupção.
- `--resume FILE` continua disponível para importar um campeão: cria uma nova
  população e desbloqueia os sete estágios. Na migração do serviço antigo, isso
  acontece uma vez por tipo; depois usamos a população completa. A população
  antiga inteira não existia nos checkpoints e não pode ser recuperada.
- População, arquétipo e currículo precisam corresponder no full resume. Um
  checkpoint inválido permite fallback para outro completo válido; se todos
  forem inválidos, o serviço interrompe. Não reinicia silenciosamente do zero.
- O snapshot anterior e todos os cérebros/relatórios da rodada antiga são
  preservados. Fitness de currículos diferentes não é comparável diretamente.

## Novos encontros

O currículo 15 mantém os sete estágios e quatro repetições. O estágio de mapas
reais alterna corredor/porta, cobertura deslocada para cada lado, Player
entrando/saindo da visão, cobertura mais larga e altura baixa. As variantes
usam os limites da sala gerada e colisão/visão reais. A plataforma baixa permite
ataque melee a partir do chão; não pressupõe que os inimigos subam escadas.
O Robot começa os encontros nas três fases de vida. As regras de recompensa
continuam na versão 5.

## Avaliação e arquivo de candidatos

- Checagem rápida por lote: seeds de desenvolvimento 7 e 1001, 360 frames,
  mapas e duelos, candidato e ativo no mesmo binário.
- Bateria ampla no primeiro lote de cada tipo e depois a cada cinco lotes desse
  tipo: seeds 7, 1001, 2027 e 4099, 1200 frames. `GDP_WIDE_EVERY` controla a
  frequência.
- Cada cenário/seed é comparado separadamente. Regressão maior que o máximo
  entre 10% do dano ativo e meio acerto do tipo impede seleção; a média não
  pode ocultar a falha. Relatórios incompletos, seeds incompatíveis e métricas
  não finitas são rejeitados.
- O ranking dos candidatos elegíveis usa dano normalizado: 60% mapas, 40%
  duelos. Movimentação é diagnóstico, não critério que compensa pior combate.
  São critérios experimentais de seleção, não garantia de equilíbrio.
- Candidatos aceitos são copiados e preservados com SHA-256. `best.json` muda
  apenas quando o score melhora dentro do mesmo protocolo. Baterias rápidas,
  amplas e pilotos têm índices separados, incluindo seeds, duração e revisão.
- A seleção gera um arquivo para revisão; os assets ativos permanecem somente
  leitura. A distribuição no jogo exige avaliação final e playtest.

As seeds finais 10007, 11003, 13001, 17011, 19001, 23003, 29009 e 31013 ficam
fora do treino e da seleção. Para avaliar explicitamente um candidato por 1800
frames, primeiro pare o serviço e use seu caminho dentro de `/data`:

```sh
docker compose stop
docker compose run --rm --entrypoint python3 trainer /opt/gdp/evaluate_final.py --role worker --brain /data/candidates/PROTOCOLO/worker/LOTE/worker_generation_000980.brain
docker compose start
```

O exemplo é um caminho ilustrativo; copie o caminho `brain` do `best.json` real.
Esse script gera relatórios em `data/final-evaluations/` e não altera o ranking.

## Piloto de população 8 versus 16

`compare_population.py` usa os mesmos pesos iniciais, duas seeds de treino
(60013 e 60017), sete gerações por braço e bateria ampla igual. Mede tempo,
dano, regressões e critérios de seleção. É um piloto curto, não uma estimativa
conclusiva da melhor população. Os dados ficam em `data/experiments/`.
Ele e o serviço usam o mesmo lock e não treinam simultaneamente.

```sh
docker compose stop
docker compose run --rm --entrypoint python3 trainer /opt/gdp/compare_population.py --role guard --brain /data/runs/guard/LOTE/guard_generation_000910.brain
docker compose start
```

## Limites e construção

Limites: 0,75 CPU, 512 MiB de RAM, 1 GiB de memória mais swap; pausa em 80 °C e
retomada em 70 °C pelo sensor `coretemp`. Sem sensor válido, interrompe. Com
menos de 1 GiB livre, não inicia novo lote. Docker logs têm rotação; checkpoints
e relatórios são preservados e consomem mais espaço com a população completa.

`build-server.sh` instala dependências somente no contêiner de build e compila
com `--parallel 1`, incluindo o teste C++ de treino. O runtime usa Python
stdlib, SDL3 e bibliotecas compatíveis com Debian Bookworm. Construção:

```sh
DOCKER_CONFIG=/DATA/AppData/gdp-training/docker-config docker build -t gdp-training:training-v2-dc5f9444d733 .
docker compose config --quiet
```

## Validação

Implantação validada em 2026-10-01: 10 testes Python passaram; teste C++ de
treino/persistência e CLI do modo novo para os cinco tipos concluíram com saída
0. No serviço real, o Officer retomou a população 1058 começando na geração
1059. O primeiro lote concluiu e gerou a bateria ampla. Registro:
`/DATA/AppData/gdp-training/deployment-training-v2-2026-10-01.json`.

O piloto do Guard terminou: médias de 16,02 s de treino com população 8 e
23,02 s com 16, sem vantagem consistente em dano para 16. Todos os quatro
braços tiveram regressão em Bhop e foram rejeitados para seleção. Mantivemos
população 8. O histórico está em `data/experiments/`.

Testes locais Python cobrem checkpoints, temperatura, relatórios incompletos,
regressões por cenário e preservação do melhor candidato. O teste C++ compara
população, pesos, fitness e holdout de uma execução contínua com uma dividida;
verifica também spawns, visibilidade no peek e acesso melee na altura.
A pausa térmica é simulada nos testes; não aquecemos o servidor até 80 °C.
A implantação e o piloto não comprovam melhora geral de gameplay: a próxima
bateria ampla e o playtest precisam confirmar os efeitos do currículo novo.

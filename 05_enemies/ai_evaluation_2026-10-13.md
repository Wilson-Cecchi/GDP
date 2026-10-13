# Cérebros candidatos para uma futura versão de Download — 13/10/2026

## Parecer

Nenhum cérebro está aprovado para substituir `assets/ai/*.brain` ou entrar no
Download. O **Worker cooperativo 19950** está apto somente a **playtest
controlado** com os outros cérebros ativos: melhorou o dano ao jogador nos oito
pares co-op e não piorou o dano nos mapas/duelos. Ainda causou **duas mortes
aliadas** nos oito encontros co-op, um risco que requer validação em jogo. O
Worker tático 26460, aprovado para playtest em 06/10, deu resultado co-op
idêntico nesta bateria e dano de duelo ligeiramente menor; não há ganho
demonstrado para preferir o cooperativo.

O Guard 5040 é **reprovado** nesta bateria: seu índice foi aprovado apenas pelo
gate **quick** de 480 frames, não pelo wide; perdeu 11 pontos de dano médio em
duelo e até 32 pontos num par `bhop`. O Officer 16100 e Engineer 17150, embora
tenham índices **wide** elegíveis no treino, são **reprovados** pela avaliação
independente: perdas fortes nos mapas e, para o Engineer, também nos duelos.
O Robot está **sem candidato cooperativo elegível**; a candidatura para nova
versão é insuficiente. Seu antigo candidato tático foi reprovado em 06/10.

| Tipo | Arquivo congelado | Geração | Gate de origem | SHA-256 |
|---|---|---:|---|---|
| Worker | `frozen/worker_coop_19950.brain` | 19950 | wide | `819a840c2bbb0c6007a42d3947c684b4f4abd93211b19001a6a30196053bb811` |
| Guard | `frozen/guard_coop_5040.brain` | 5040 | quick somente | `3f3a9b4cf89a67dfced7544951e9e89477fed38558a6cc7caaf1c9bece695a40` |
| Officer | `frozen/officer_coop_16100.brain` | 16100 | wide | `dd4efbe6dde27029600c3c1af225a405e680b9c5d484609a3bd78e7ae2484abb` |
| Engineer | `frozen/engineer_coop_17150.brain` | 17150 | wide | `b5a67df0b14aed7d4cbe582691f85d2729adc90a7dfc2723dca7405bc427500f` |

Os quatro são `GDP_ENEMY_BRAIN 4`, contrato 3, seed de treino 20261009.
Os índices `best.json` foram copiados sob `build/ai-evaluation-20261013/`.
O `best.json` wide do Guard não existia na consulta. O serviço continuava em
treino; esses arquivos arquivados são um recorte congelado, não uma afirmação
de que são os últimos cérebros produzidos.

## Protocolo independente

- Checkout de avaliação: `e1765a3765bdbc4d12453ea9866617ac20e2f1bf`.
  O treino ZimaOS registrou a revisão
  `dcfbb0b66fd4240e156a8ad3d850700a959064d6+coop-service-dfdba50685ec-c8c7c30b44df`:
  **a revisão de treino difere da revisão avaliada**. O serviço alterna Worker,
  Guard, Officer e Engineer; não treina Robot nesse contrato.
- Mesmo executável para ativos e candidatos:
  `build/debug/gdp_ai_gameplay_eval`, reconstruído com
  `cmake --build build/debug --target gdp_ai_gameplay_eval -j2`, SHA-256
  `664d0f09fdbd2e2b5a4bc124565542caf9407fccefb081c210567850cd584608`.
  Carregamento do formato 4 confirmado por uma execução `--team` de 1 frame;
  o CSV registrou geração 19950. O binário é **debug**; não mede desempenho
  da versão Download.
- Oito seeds novas congeladas antes de ler os resultados: `101003`, `103009`,
  `107011`, `109001`, `113017`, `127031`, `131071`, `137029`; SHA-256 do
  arquivo `seeds.txt`:
  `5f02f3f4697c88052daa0e03e47c867fb7c2e759a3ebfa511dea5e74492e38dd`.
  Até 1800 frames por encontro (30 segundos simulados). As seeds antigas
  `9001`, `9002`, `12011`, `14009` pertencem apenas aos gates ZimaOS.
- Cada execução usa `--seed N --frames 1800`, acrescido de `--duel` ou `--team`
  quando aplicável e `--brain TIPO=ARQUIVO` somente para o candidato. Todos os
  demais cérebros, inclusive o aliado no co-op, permanecem ativos. São 8 seeds
  por perfil, 32 pares de mapas, 16 de duelos e 8 pares por cenário co-op
  afetado. O Guard participa dos três cenários co-op. Total: **296 pares**
  ativo/candidato, **3264 linhas CSV** incluindo as linhas de controle
  `scripted`. Os pares completos por cenário/seed, incluindo tiros, acertos,
  dano e mortes aliadas, holds, suporte, vitória e tempo, estão em
  `build/ai-evaluation-20261013/paired.csv`; brutos e hashes em
  `raw/*.csv`, `results.json` e `manifest.json`. Todos os CSVs tiveram
  contagem esperada, sem campos vazios ou números não finitos. A bateria
  completa medida novamente levou 16,96 s nesta máquina (RSS máximo 21 MiB).

Hashes dos ativos usados: Worker
`23220e82047115191c6552d8102754a256e2e31b319a5fb904ddb98c555dfb6c`,
Guard `a4fb3621f4fdecbaa9a83a84cd6c915eb2fc2ae75f7cec5b43ad249323017261`,
Officer `df8556cda163e90278b833412781cc69b5d7c0f094315ad34ed6de05b1d67b34`,
Engineer `93bf1bfc554f4a3885fd7e875e29adf974bf8763f4fa2e42f5bf33d3482d641d`,
Robot `78d57f01aacca39fd03c3fe06e268c0a52094e5d5beb9c08035757efa15c974a`.
O Worker tático anterior foi congelado em
`809ec05994841fcded8c15606a03f6bcdacee7cb4662a17554954633700fae18`.

## Resultados pareados

Valores são médias **ativo → candidato**. Dano ao jogador maior favorece o
inimigo; dano aliado menor favorece a segurança. `Mapas` inclui campo aberto,
cobertura central, altura e grupo; `duelos` inclui movimento no chão e bhop.
Os mapas têm teto de 100 de dano e o jogador não atira; tempo até derrota
complementa o empate nesse teto. Nos duelos o jogador automatizado atira e
venceu todos os 80 pares de cada política analisada.

| Perfil | Dano mapas (32) | Dano duelos (16) | Dano co-op por cenário (8 cada) | Dano aliado co-op total | Mortes aliadas co-op total |
|---|---:|---:|---|---:|---:|
| Worker tático 26460 | 75,0 → 75,0 | 3,8 → 5,6 | `firing_lane` 17,0 → 33,0 | 82 → 76 | 5 → 2 |
| Worker coop 19950 | 75,0 → 75,0 | 3,8 → 6,2 | `firing_lane` 17,0 → 33,0 | 82 → 76 | 5 → 2 |
| Guard coop 5040 | 98,9 → 98,2 | 14,5 → 3,5 | `firing_lane` 17,0 → 20,5; `officer_guard` 43,8 → 55,8; `engineer_guard` 68,5 → 80,8 | 114 → 56 | 5 → 0 |
| Officer coop 16100 | 100,0 → 69,7 | 6,4 → 9,4 | `officer_guard` 43,8 → 72,0 | 0 → 8 | 0 → 0 |
| Engineer coop 17150 | 71,9 → 46,9 | 7,5 → 0,3 | `engineer_guard` 68,5 → 78,8 | 32 → 24 | 0 → 0 |

| Perfil | Disparos/acertos no duelo | Disparos/acertos inimigos no co-op | Holds co-op | Suporte co-op | Vitórias do inimigo no co-op |
|---|---|---|---:|---:|---:|
| Worker coop | 0/0 → 0/0 | 8,0/2,1 → 8,0/3,9 | 79,9 → 64,4 | 0 → 0 | 0 → 0 |
| Guard coop | 6,0/1,8 → 3,0/0,4 no duelo | 13,1/6,1 → 13,1/7,3 | 40,9 → 1,7 | 1,8 → 1,8 | 0 → 0 |
| Officer coop | 4,0/1,1 → 4,0/1,6 no duelo | 15,0/6,0 → 16,4/10,1 | 0 → 13,2 | 2,0 → 2,8 | 0 → 2 |
| Engineer coop | 3,6/1,5 → 1,4/0,1 no duelo | 16,4/10,2 → 15,8/12,0 | 42,9 → 49,0 | 3,5 → 3,6 | 0 → 0 |

Na primeira linha da segunda tabela, Worker não usa projéteis (`enemy_shots`
igual a zero); seus acertos de contato no duelo foram 0,4 → 0,6 por par.
Tiros/acertos do jogador no co-op, médias ativo → candidato: Worker
12,2/12,2 → 12,6/12,6; Guard 14,2/14,2 → 14,5/14,5; Officer
15,0/15,0 → 14,8/14,8; Engineer 15,5/15,5 → 15,6/15,6. A mira
automatizada acerta praticamente todos os tiros; isso não representa
precisão humana.

| Perfil | Frames médios nos mapas | Frames médios nos duelos | Frames médios co-op |
|---|---:|---:|---:|
| Worker coop | 1000 → 933 | 157 → 157 | 452 → 466 |
| Guard coop | 688 → 1099 | 274 → 274 | 530 → 540 |
| Officer coop | 1171 → 1457 | 235 → 235 | 559 → 554 |
| Engineer coop | 1474 → 1446 | 233 → 235 | 579 → 585 |

Para todos os perfis, duração, outcomes e métricas de cada seed estão no CSV
pareado, evitando esconder variações por média.

### Piores pares e interpretação

- **Worker:** nenhuma queda de dano nos 56 pares de cada candidato. Em
  `firing_lane/127031`, dano ao jogador foi 18 → 68 (maior ganho). Em co-op,
  ambos os Workers ainda deram 76 de dano aliado acumulado e duas mortes;
  o ganho de combate não resolve esse risco. Os oito resultados co-op do
  tático e cooperativo foram iguais, portanto o contrato novo não demonstrou
  benefício adicional nesse cenário.
- **Guard:** 12 quedas nos mapas/duelos. Em `bhop/103009`, 32 → 0; em
  `open/131071`, 100 → 80. O co-op melhorou dano aliado e eliminou cinco
  mortes, mas o teste de duelo mostra que o gate quick não basta para versão.
- **Officer:** 22 quedas em mapas; `center_cover/127031`, 100 → 30. No co-op,
  `officer_guard/131071` foi 42 → 100, mas houve uma ocorrência de dano aliado
  de 8 em `103009`. O jogador perdeu 2 dos 8 co-op com o candidato, contra
  zero com o ativo.
- **Engineer:** 20 quedas nos mapas e 13 nos duelos; `elevated/103009`,
  100 → 20, e `bhop/103009`, 15 → 0. O co-op melhorou em 5/8 pares, porém
  houve uma piora de dano aliado de 8 em `engineer_guard/137029`. Nos mapas,
  as ações de suporte médias caíram de 1,8 para zero.

## Elegibilidade e limite de aprovação

Os índices wide do ZimaOS passaram apenas seu gate de time, com quatro seeds
e 1200 frames; o Guard passou somente o quick, com duas seeds e 480 frames.
Essa elegibilidade **técnica** não representa aprovação de qualidade ou
segurança na campanha. O fixture independente usa salas geradas Start/Boss,
jogador automatizado e companheiros ativos; não mede percepção do jogador,
ritmo, desafio desejado, mapas completos ou comportamento de longo prazo.
É necessária aprovação humana em jogo para qualquer futuro pacote Download,
especialmente porque ainda há mortes aliadas com Worker. Nenhum asset ativo,
serviço ou checkpoint foi alterado e nenhum cérebro foi promovido.

Para esse playtest, `build/ai-evaluation-20261013/playtest-worker-coop/`
contém o Worker 19950 e cópias dos outros quatro cérebros ativos. O smoke test
`build/debug/gdp --test-brain-dir build/ai-evaluation-20261013/playtest-worker-coop --headless --frames 60`
terminou com código 0 e registrou Worker 19950 e os
quatro demais na geração 200. Isso valida carregamento isolado, não a qualidade
do combate em uma partida. O diretório de teste é ignorado pelo Git e não faz
parte do pacote de Download.

## Reproduzir e verificar

```bash
cmake --build build/debug --target gdp_ai_gameplay_eval -j2
build/debug/gdp_ai_gameplay_eval --seed 101003 --frames 1800 --team \
  --brain worker=build/ai-evaluation-20261013/frozen/worker_coop_19950.brain
python3 build/ai-evaluation-20261013/run_eval.py
python3 build/ai-evaluation-20261013/analyze.py
sha256sum build/debug/gdp_ai_gameplay_eval assets/ai/*.brain \
  build/ai-evaluation-20261013/frozen/*.brain
git diff --check
git status --short
```

Os scripts e cópias de brain ficam somente no diretório de artefatos ignorado.
`run_eval.py` valida as contagens e rejeita campos vazios ou NaN/Inf. O
`paired.csv` contém exatamente um par por perfil, cenário e seed. A execução
não congela o serviço: uma avaliação futura deve copiar novos candidatos,
congelar novas seeds e repetir o protocolo no binário dessa revisão.

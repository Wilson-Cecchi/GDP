# Avaliação das IAs cooperativas — 25/10/2026

## Estado e decisão

O serviço `gdp-perception-v4-r2` continua treinando no ZimaOS. Seus índices
`best.json` continham candidatos amplos para Worker 1820 e Robot 2030, apenas
um candidato rápido para Officer 17850 e nenhum candidato elegível para Guard
ou Engineer na consulta deste dia. O responsável aprovou a prévia Worker/Boss
em partida; Worker 1820 e Robot 2030 foram promovidos a `assets/ai/` nesta
fase. O Download público continua separado desta mudança.

| Papel | Candidato | SHA-256 do cérebro | Avaliação final local | Decisão |
| --- | ---: | --- | --- | --- |
| Worker | 1820 | `c5f4a9dc70fbc5d7177a082583fe9756032a86a4a240dea13bade7567eb10a5b` | score 1,022; 0 regressões | Ativo após playtest |
| Guard | — | — | sem candidato elegível | Manter ativo |
| Officer | 17850 | `e13dd309516e6b52ce71e279a3f9606af9fc5b329fe6adedab3076eda3f3278f` | score 1,274; 10 regressões em mapas | Reprovado |
| Engineer | — | — | sem candidato elegível | Manter ativo |
| Robot | 2030 | `8e51089b9622d0718d124d8a9ccc8f4caa6e003d3e0f5881ef68152520926df1` | score 1,263; 0 regressões | Ativo após playtest |

O score não substitui as regressões por cenário. O Officer melhorou em duelo e
equipe, mas perdeu dano em `center_cover` e `open` em dez pares de semente;
por isso não entra na prévia. O Robot causou em média 88,19 de dano ao Player
em `boss_helpers`, contra 57,38 do ativo, sem fogo amigo nessa bateria. Essa
diferença de pressão foi aceita no playtest antes da promoção.

## Protocolo reproduzível

- Candidatos congelados pelos hashes acima, copiados da área de candidatos do
  serviço v4-r2. Os ativos anteriores foram lidos de `assets/ai/` no mesmo
  executável e preservados em `build/ai-playtest-2026-10-25/active/`.
- Binário local `build/release/gdp_ai_gameplay_eval`, SHA-256
  `dc416746a2ec64c1f60ccfcdfd8c2d70bf23eaee24adbbb940df87bf4fbed000`,
  compilado sobre `6bbf81a` com as correções de runtime deste workspace.
- Oito sementes finais `10007`, `11003`, `13001`, `17011`, `19001`, `23003`,
  `29009` e `31013`, fora da seleção; 1800 quadros por execução. Para cada
  papel e semente, ativo/candidato passaram por mapas, duelo, equipe com autoria
  de fogo amigo e investigação de perda de visão.
- Relatórios completos: `05_enemies/evaluations/2026-10-25/`.

As comparações são automatizadas e determinísticas. Não medem sensação em
partida, dificuldade de uma campanha completa nem a combinação de todos os
tipos novos ao mesmo tempo. A aprovação em partida foi uma decisão humana
posterior a esta bateria.

## Ajustes de runtime nesta fase

- `FireLaneBlocked` e o disparo agora consultam o mesmo cano, orientação e
  distância, inclusive os encaixes dos quatro atiradores. Antes, o cérebro
  recebia um sinal de corredor de tiro calculado de um cano central fictício.
- Um aviso novo de aliado substitui lembrança direta antiga; contato visual
  direto recente continua prioritário.
- A recuperação temporária da política roteirizada considera disparo real,
  aproximação e espera legítima por recarga/cadência. Apenas mirar sem conseguir
  disparar deixou de contar como ataque eficaz.

Os testes `enemy_sensors_systems`, `combat_systems`, `active_enemy_brain_catalog`,
`active_enemy_brains_headless` e `ai_gameplay_eval_deterministic` passaram. A
prévia mista também iniciou por 180 quadros em headless e por 120 quadros na
Quadro K2100M, em ULTRA, com os cinco cérebros carregados. O responsável
aprovou a prévia em partida em 25/10/2026.

Após a promoção aprovada, a suíte Release completa passou: 80/80 testes,
incluindo a inicialização com os cérebros distribuídos e os testes OpenGL.

## Prévia local

Os cinco arquivos da prévia aprovada estão em
`build/ai-playtest-2026-10-25/worker1820_robot2030/`; os ativos anteriores
estão em `build/ai-playtest-2026-10-25/active/`. Para jogar com os novos
cérebros ativos na NVIDIA:

```sh
cd /home/wilson/GDP/GDP-3
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/release/gdp
```

Para usar a arena, troque o executável por `./build/playtest/gdp` e acrescente
`--test-mode`. Guard, Officer e Engineer permanecem nos cérebros anteriores.
Nenhum pacote de Download foi publicado nesta fase.

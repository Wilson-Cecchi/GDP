# Testing — Performance budgets

> **Estado:** instrumentação inicial implementada na Fase 117, primeira
> calibração por hardware aplicada na Fase 118, culling conservador na Fase 119,
> tempo real da GPU na Fase 120, protocolo reproduzível na Fase 128 e instancing
> estrutural na Fase 130. A Fase 115 introduziu o primeiro budget rígido do passe
> de sombras, especializado no shader na Fase 131.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Métricas CPU

Frame, update, world generation e preparação visual são medidos em milissegundos.
O frame conserva média, percentil 95, pior caso, FPS estimado e percentual acima
do budget de `16,67 ms`, numa janela móvel de 120 amostras. IA isolada permanece
pendente de instrumentação caso o update seja identificado como gargalo.

## Métricas GPU

Submissão do renderer, execução da GPU e espera de apresentação são medidas
separadamente. Quatro timer queries assíncronas evitam forçar sincronização; a
média ignora aquecimento, slots ainda ocupados e drivers sem suporte. Draw calls
e triângulos vêm do caminho real de draw; contagem de entradas nos caches de
mesh/textura complementa o diagnóstico. Memória aproximada de texturas/buffers
permanece pendente até uma medição exigir esse detalhamento.

Sombras analíticas usam no máximo duas luzes e oito AABBs por frame. O custo
combinatório máximo é 16 interseções de segmento por fragmento com contribuição
difusa, contra 128 na implementação funcional sem seleção da Fase 114.

## Resources

Inimigos, props e salas visíveis, luzes, luzes de sombra, oclusores selecionados,
entradas de mesh e entradas de textura são observáveis por frame. Cache hit rate
permanece disponível no cache e pode ser promovido ao overlay se as entradas
indicarem pressão real.

## Regra

Não otimizar por intuição. Medir em cenas de stress e corrigir regressões reais.

A seleção de sombra é observável por `ShadowBudgetResult`: luzes ativas,
candidatos recebidos e oclusores efetivamente enviados. Isso permite instrumentar
cenas futuras sem misturar telemetria ao shader.

## Stress cases

Sala cheia de props; maior encontro; Boss; muitas luzes; cache frio; cache quente.

## Coleta da Fase 117

- `F3` alterna o overlay durante uma run sem pausar o jogo;
- a cor usa o P95: verde até 17 ms, laranja até 34 ms e vermelho acima disso;
- `--headless --frames 900 --seed 12345` registra o custo determinístico de CPU;
- VSync desligado permite observar capacidade bruta; ligado inclui a espera de
  apresentação explicitamente na coluna `WAIT`;
- os futuros presets de desempenho serão escolhidos somente após comparar uma
  sala comum, o maior encontro e o Boss em pelo menos um hardware fraco e um
  hardware forte.

## Perfis da Fase 118

Uma coleta no ThinkPad W541 com Quadro K2100M, no caminho visual legado,
registrou 9 FPS, média de 115 ms, P95 de 127 ms, pior frame de 201 ms,
`UPDATE 2 ms`, `RENDER 84 ms` e `WAIT 29 ms`. A cena apresentou 273 draws,
8.420 triângulos, oito luzes e budget de sombra 2×8. Isso localizou a pressão no
caminho gráfico e justificou perfis que não alteram lógica ou conteúdo.

| Perfil | Escala | Luzes | Sombras | Profundidade de salas | Material detalhado |
|---|---:|---:|---:|---:|---|
| POTATO | 50% | 2 | 0×0 | 1 | não |
| LOW | 67% | 4 | 0×0 | 1 | não |
| MEDIUM | 85% | 6 | 1×3 | 2 | sim |
| HIGH | 100% | 8 | 1×6 | 2 | sim |
| ULTRA | 100% | 8 | 2×8 | 2 | sim |

O HUD e os menus não usam a escala interna. O próximo playtest no W541 deve
comparar `POTATO`, `LOW` e `MEDIUM` na mesma cena com `F3`, preferencialmente
com VSync desligado, antes de promover qualquer perfil automaticamente.

## Culling da Fase 119

Salas conectadas, corredores, inimigos, pickups, props e impactos fora da câmera
são removidos antes da fila de draw. O filtro usa bounds conservadores e margem
angular; a sala atual e entradas inválidas falham abertas. A profundidade do
perfil é compartilhada entre geometria e entidades. As luzes permanecem fora
desse filtro direcional para não remover iluminação que alcança a visão.

No playtest, girar lentamente diante de uma porta aberta não pode revelar
buracos, entidades sem shell ou aparecimento tardio dentro do campo visual. Com
`F3`, os contadores de entidades e draws devem cair ao olhar para uma parede e
subir novamente ao retornar para a sala, sem mudança nos ticks de gameplay.

## Timer da Fase 120

`CPU RENDER` mede a chamada do renderer no processador; `GPU` mede comandos
executados pela placa; `WAIT` mede `SDL_GL_SwapWindow`. Como a leitura da GPU é
assíncrona, sua média representa resultados de frames anteriores e não precisa
somar exatamente ao frame atual. Se `GPU --` persistir depois de alguns frames,
o driver não publicou timer queries e as métricas de CPU continuam válidas.

## Protocolo reproduzível da Fase 128

`--benchmark PROFILE` isola a coleta das preferências persistidas e usa:

- seed determinística (`12345` no executor dos cinco perfis);
- 1280×720 em janela, VSync desligado e áudio sem dispositivo físico;
- um fixed update por frame e nenhuma entrada de gameplay;
- 240 frames por padrão, preservando no resumo a janela móvel final de 120;
- uma linha `BENCHMARK` legível por máquina com tempos e carga do renderer.

Executar a matriz comparável em build Release:

```bash
cmake --preset release
cmake --build --preset release --target graphics-benchmark
```

O runner mede os perfis na ordem `POTATO`, `LOW`, `MEDIUM`, `HIGH` e `ULTRA`
com a mesma seed. Para variar a amostra de forma explícita:

```bash
cmake -DGDP_EXECUTABLE=build/release/gdp \
  -DGDP_SEED=67890 -DGDP_FRAMES=360 \
  -P tools/run_graphics_benchmark.cmake
```

Resultados produzidos com `GDP_OFFSCREEN=ON` verificam inicialização, formato e
comparabilidade estrutural, mas não são baseline de GPU. Um baseline aceito deve
registrar build/commit, sistema, CPU, GPU/driver, perfil, seed, resolução e a
linha completa emitida pelo executável. A vista inicial reproduzível detecta
regressões gerais; sala cheia, maior encontro e Boss continuam sendo coletas
separadas antes de mudar budgets ou promover um perfil automaticamente.

### Baseline W541 validado em 2026-09-20

O protocolo foi executado no ThinkPad W541 com Quadro K2100M, commit `edc791e`,
seed `12345`, 240 frames e 1280×720. Cada linha abaixo resume a janela final de
120 amostras:

| Perfil | FPS | Média | P95 | Pior | Acima de 16,67 ms | GPU | Draws |
|---|---:|---:|---:|---:|---:|---:|---:|
| POTATO | 79,44 | 12,59 ms | 15,13 ms | 15,78 ms | 0,00% | 10,62 ms | 80 |
| LOW | 74,76 | 13,38 ms | 16,96 ms | 22,58 ms | 6,67% | 11,48 ms | 96 |
| MEDIUM | 29,35 | 34,07 ms | 40,41 ms | 50,25 ms | 100,00% | 31,95 ms | 265 |
| HIGH | 20,19 | 49,53 ms | 54,61 ms | 56,34 ms | 100,00% | 47,33 ms | 281 |
| ULTRA | 19,05 | 52,48 ms | 58,50 ms | 62,91 ms | 100,00% | 50,43 ms | 281 |

`UPDATE` permaneceu entre 0,07 e 0,11 ms. O salto ocorre no trabalho gráfico e
na apresentação: `POTATO` e `LOW` usam duas salas, zero sombras e shader de baixo
custo, enquanto `MEDIUM` passa a cinco salas, 265 draws, materiais completos e
sombras. Assim, `POTATO` é o perfil estável de 60 FPS para esse hardware; `LOW`
é a alternativa visual ainda próxima do budget, e `MEDIUM` ou superior não deve
ser promovido automaticamente no W541. O próximo ganho deve reduzir submissões
e trabalho repetido de geometria antes de diminuir novamente a qualidade.

## Instancing da Fase 130

O agrupamento atua somente em superfícies opacas de salas e corredores com
estado visual idêntico. A seed `12345` mantém os mesmos triângulos e registra:

| Perfil | Draws anteriores | Draws atuais | Draws evitados | Lotes | Instâncias |
|---|---:|---:|---:|---:|---:|
| POTATO | 80 | 67 | 13 | 8 | 21 |
| LOW | 96 | 83 | 13 | 8 | 21 |
| MEDIUM | 265 | 185 | 80 | 16 | 96 |
| HIGH | 281 | 201 | 80 | 16 | 96 |
| ULTRA | 281 | 201 | 80 | 16 | 96 |

Esses contadores são determinísticos e independem do driver. Os tempos
offscreen continuam inadequados como baseline de hardware; o ganho real deve ser
medido novamente no W541 pelo protocolo da Fase 128. A regressão OpenGL renderiza
o mesmo corredor com instancing desligado e ligado, exige pixels byte a byte
iguais, triângulos iguais e a diferença exata de draws reportada por `SAVE`.

### Validação no W541 em 2026-09-21

O mesmo protocolo foi repetido no ThinkPad W541 após o commit `93804f4`. A
redução estrutural apareceu integralmente no driver real: 13 draws evitados em
`POTATO/LOW` e 80 em `MEDIUM/HIGH/ULTRA`, com triângulos inalterados.

| Perfil | FPS anterior | FPS atual | Média anterior | Média atual | GPU anterior | GPU atual | P95 atual |
|---|---:|---:|---:|---:|---:|---:|---:|
| POTATO | 79,44 | 80,05 | 12,59 ms | 12,49 ms | 10,62 ms | 10,59 ms | 16,10 ms |
| LOW | 74,76 | 66,33 | 13,38 ms | 15,08 ms | 11,48 ms | 12,93 ms | 19,50 ms |
| MEDIUM | 29,35 | 30,14 | 34,07 ms | 33,18 ms | 31,95 ms | 31,06 ms | 37,58 ms |
| HIGH | 20,19 | 21,02 | 49,53 ms | 47,56 ms | 47,33 ms | 45,49 ms | 53,99 ms |
| ULTRA | 19,05 | 18,85 | 52,48 ms | 53,04 ms | 50,43 ms | 50,97 ms | 65,94 ms |

Uma única passagem mostra ganho modesto em `MEDIUM` e `HIGH`, estabilidade em
`POTATO` e regressão acompanhada de picos em `LOW` e `ULTRA`. Como a carga de
geometria não mudou e o tempo continua concentrado em `GPU/PRESENT`, essa
amostra valida a integração e a redução de submissões, mas não sustenta promover
um perfil nem declarar ganho uniforme de FPS. Comparações de desempenho menores
que a variação observada precisam de múltiplas passagens antes de orientar outro
budget.

## Especialização de sombras da Fase 131

O renderer nunca envia mais de oito oclusores, mas o shader completo ainda
declarava e compilava um laço com teto 16. Há agora variantes com capacidade 3,
6 e 8, selecionadas pelo número efetivo de oclusores do frame. Assim, o budget
`MEDIUM` cabe normalmente em 3, `HIGH` em até 6 e `ULTRA` em até 8 sem alterar
os limites de qualidade ou o resultado visual.

O contador `shadow_capacity` torna a escolha observável no benchmark e
`SHADOW CAP` faz o mesmo no F3. A regressão OpenGL compara a capacidade 3 com a
referência 8 e exige pixels byte a byte idênticos; casos com quatro e sete
oclusores garantem a seleção e execução das variantes 6 e 8. A próxima matriz no
W541 deve avaliar o ganho real, pois somente o driver da K2100M pode demonstrar
se os limites estáticos menores reduzem o custo do fragment shader.

Na cena determinística da seed `12345`, `POTATO/LOW` registram capacidade zero e
`MEDIUM/HIGH/ULTRA` selecionam capacidade 3, pois os três oclusores efetivos já
cabem na menor variante completa. Esses valores estruturais foram confirmados
offscreen; os tempos dessa execução não são baseline de hardware.

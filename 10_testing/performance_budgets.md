# Testing — Performance budgets

> **Estado:** instrumentação inicial implementada na Fase 117 e primeira
> calibração por hardware aplicada na Fase 118. A Fase 115 introduziu o primeiro
> budget rígido do passe de sombras.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Métricas CPU

Frame, update, world generation e preparação visual são medidos em milissegundos.
O frame conserva média, percentil 95, pior caso, FPS estimado e percentual acima
do budget de `16,67 ms`, numa janela móvel de 120 amostras. IA isolada permanece
pendente de instrumentação caso o update seja identificado como gargalo.

## Métricas GPU

Submissão do renderer e espera de apresentação são medidas separadamente, sem
forçar sincronização da GPU. Draw calls e triângulos vêm do caminho real de
draw; contagem de entradas nos caches de mesh/textura complementa o diagnóstico.
Tempo GPU por query e memória aproximada de texturas/buffers permanecem pendentes
até uma medição exigir esse detalhamento.

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

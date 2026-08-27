# Testing — Performance budgets

> **Estado:** especificação técnica inicial do novo GDP.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Métricas CPU

Frame ms, fixed update ms, AI ms, world generation ms, mesh generation ms e texture generation ms.

## Métricas GPU

Render ms, draw calls, triangles e memória aproximada de texturas/buffers.

## Resources

Entidades ativas, cached meshes, cached textures e cache hit rate.

## Regra

Não otimizar por intuição. Medir em cenas de stress e corrigir regressões reais.

## Stress cases

Sala cheia de props; maior encontro; Boss; muitas luzes; cache frio; cache quente.

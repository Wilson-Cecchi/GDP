# Testing — Performance budgets

> **Estado:** budgets gerais permanecem em especificação; a Fase 115 aplicou e
> validou o primeiro budget rígido do passe de sombras.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Métricas CPU

Frame ms, fixed update ms, AI ms, world generation ms, mesh generation ms e texture generation ms.

## Métricas GPU

Render ms, draw calls, triangles e memória aproximada de texturas/buffers.

Sombras analíticas usam no máximo duas luzes e oito AABBs por frame. O custo
combinatório máximo é 16 interseções de segmento por fragmento com contribuição
difusa, contra 128 na implementação funcional sem seleção da Fase 114.

## Resources

Entidades ativas, cached meshes, cached textures e cache hit rate.

## Regra

Não otimizar por intuição. Medir em cenas de stress e corrigir regressões reais.

A seleção de sombra é observável por `ShadowBudgetResult`: luzes ativas,
candidatos recebidos e oclusores efetivamente enviados. Isso permite instrumentar
cenas futuras sem misturar telemetria ao shader.

## Stress cases

Sala cheia de props; maior encontro; Boss; muitas luzes; cache frio; cache quente.

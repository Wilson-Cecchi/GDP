# Interface — Debug UI

> **Estado:** painel funcional de desempenho implementado nas Fases 117–120;
> overlays espaciais detalhados permanecem planejados.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Painéis

Seed tree; room ID/role/theme; connectors; spawn zones; EnemyVisualGenome; part IDs;
material IDs; asset hash; FPS; tempos de CPU/GPU/present; draw calls; cache.

## Overlays

Collision, room bounds, connector normals, pathfinding, AI target, semantic regions e light bounds.

## Reprodução

Toda tela de debug deve facilitar copiar seed e IDs necessários para reproduzir um bug.

## Build

Pode ser removida/desativada no Release sem alterar comportamento do jogo.

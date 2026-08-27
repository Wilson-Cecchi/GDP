# World — Validação estrutural e de jogabilidade

> **Estado:** gates de módulos, graph, geometry, connectors, corridors e
> navegação lógica implementados até a Fase 5; spawns/conteúdo/gameplay entram
> nas fases seguintes.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `README(6).md — Validação procedural`
- `architecture.md`

## Gates

```text
GRAPH → GEOMETRY → CONNECTORS → NAVIGATION → SPAWNS → GAMEPLAY
```

## Graph

START único, BOSS quando exigido, conectividade, main path e nenhuma sala obrigatória isolada.

## Geometry

Sem overlap inválido, gap válido, corridor sem atravessar sala e clearance respeitado.

## Connectors

Edge resolvido, required usado, tipo/dimensão válida e porta associada.

## Navigation

BFS/navegação simplificada deve alcançar START→salas obrigatórias→BOSS.

## Spawns

Player/enemy/loot/props nunca dentro de parede, porta, fora do piso ou bloqueando única saída.

## Gameplay

Checar recursos mínimos, Boss único, objetivos possíveis e dependências de chave/porta sem ciclo impossível.

## Failure enum

```cpp
enum class GenerationFailure {
    GraphDisconnected, RoomOverlap, ConnectorUnresolved,
    CorridorInvalid, SpawnInvalid, ObjectiveImpossible, ResourceConstraint
};
```

## Métricas

Accepted, regenerated, reason, attempt count e generation time.

## Headless gate

Meta de maturidade: 1.000+ mundos, zero crashes e zero mundos aceitos violando invariantes.

O gate de 1.000 mundos foi atingido na Fase 5 em Debug, Release e ASan/UBSan.
Cada candidato aceito possui graph conectado, rooms sem overlap, duas pontas de
connector por edge, exatamente um corridor válido por edge e nenhuma interseção
silenciosa entre corredores.

## Dumps

Seed, attempt, graph, transforms, connector pairs, corridor paths e reason.

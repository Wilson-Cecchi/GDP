# World — Map Graph

> **Estado:** grafo base, roles, themes, depth, main path, branches e loop budget
> implementados e validados na Fase 5.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `README(6).md — Grafo do mapa`
- `architecture.md`

## Objetivo

Definir progressão e conectividade antes da geometria.

## Dados

```cpp
struct RoomNode { RoomNodeID id; RoomRole role; RoomTheme theme; int depth; };
struct MapEdge { EdgeID id; RoomNodeID a,b; bool mainPath; };
```

## Geração base

```text
base conectada → MST → loops controlados → roles → branches/secrets
```

A implementação parte de uma chain autoritativa START→BOSS, anexa branches ao
Hub e gasta um `loopBudget` explícito sem self-edge, duplicata ou grau maior que
quatro. Oito salas são o perfil inicial, com duas branches e um loop.

## Regras

- exatamente um START;
- exatamente um BOSS quando o andar exigir;
- START alcança BOSS;
- SECRET nunca é único caminho obrigatório;
- HUB precisa justificar múltiplas rotas;
- ARENA marca encontro controlado;
- DEAD_END pode conter recompensa/ambientação.

## Depth

Calcular BFS a partir de START. Depth alimenta dificuldade, loot e encontros.

## Main path

A nova versão armazena explicitamente o caminho START→BOSS depois do grafo final.

## Loops

Usar `loopBudget`, não probabilidade ilimitada por par.

## Invariantes

Conectado, sem self-edge, sem duplicata acidental, depths coerentes.

## Debug/testes

Export DOT e medir nós, grau, loops, profundidade, branches e secrets em lotes de seeds.

`graphToDot()` fornece o dump técnico. O validator verifica IDs, endpoints,
conectividade, contagens START/BOSS, depth, Hub e cada passo do main path.

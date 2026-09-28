# World — Pipeline de geração

> **Estado:** pipeline estrutural até commit implementado e validado na Fase 5;
> população lógica inicial de entidades implementada na Fase 6.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `architecture.md — World Generator`
- `README(6).md — Room System/Grafo`

## Propriedade arquitetural

Este módulo pertence ao GDP, não à WKC Forge. Embora grafo e corredores tenham
partes conceitualmente reutilizáveis, os contratos atuais dependem diretamente
de `RoomRole`, `RoomTheme`, módulos, encontros, loot, props e população próprios
do jogo. Uma extração só deve ocorrer quando existir um segundo consumidor e um
contrato genérico comprovado; até lá, o alvo permanece `gdp_world` e a API,
`gdp::world`.

## Pipeline

```text
MASTER SEED
→ MAP GRAPH
→ ROOM ROLES
→ ROOM THEMES
→ ROOM MODULES
→ PLACEMENT
→ CONNECTOR ASSIGNMENT
→ CORRIDOR ROUTING
→ DOORS
→ STRUCTURAL VALIDATION
→ ROOM CONTENT
→ GAMEPLAY VALIDATION
→ COMMIT
```

## Candidate world

Gerar em estrutura temporária. O jogo só recebe um `World` depois de todos os gates.

```cpp
for (int attempt=0; attempt<maxAttempts; ++attempt) {
    Seed s=deriveSeed(worldSeed, SeedDomain::World, attempt);
    auto candidate=generateCandidate(s);
    if (validate(candidate)) return commit(candidate);
}
```

A tentativa usada faz parte do resultado reproduzível.

`generateWorld()` usa até 32 tentativas derivadas e só retorna um `World` após
`validateWorld()`. A saída preserva master seed, attempt seed, versão, tentativa
aceita, graph, rooms e corridors. Falhas distinguem graph, module, overlap,
connector e corridor.

## Separação lógica/física

`MapEdge → ConnectorPair → CorridorPath`. Uma aresta lógica não é geometria.

Essa separação está explícita nos tipos atuais. Placement usa espaçamento mínimo
e rotação por sala; connector selection escolhe a normal disponível mais
alinhada ao destino antes de consumir a edge.
O caminho principal agora escolhe uma direção cardinal inicial e faz uma ou
duas curvas de 90 graus em índices internos derivados da seed. As branches
procuram posição livre ao redor do nó pai e favorecem proximidade aos seus
vizinhos lógicos.
Cada candidato é rejeitado se houver sobreposição, corredor inválido ou rota
individual acima de 17 unidades; nenhuma sala é deslocada indefinidamente para
abrir espaço. O gate de 1.000 seeds verifica conectividade, variedade de
direções/ordem e o limite dos corredores.

## Saída

`World` contém seed/version, graph, rooms e corridors. Depois do commit
estrutural, `populateInitialEntities()` preenche em cada sala os `EntityId`s
possuídos pelo manager central; IA, loot coletável e gameplay permanecem
separados desse gate geométrico. A instância preserva suas `lootZones` e
`enemyZones`, `lootZones` e `propZones` já transformadas para world space;
população/runtime posteriores derivam encontros, loot, props, eventos e iluminação em
substreams independentes sem alterar o fingerprint estrutural.

## Herança de design

Preservar MST, loops controlados, roles, templates, connectors, corredores reto/L/Z, rejeição/regeneração e validação em lotes. A implementação é nova.

## Falha

Esgotou tentativas: dump completo + erro explícito em Debug. Release só usa fallback determinístico se essa política for decidida no produto.

## Testes

Escalar 1→100→1.000→10.000 seeds e registrar taxa/motivo de regeneração.

O gate atual executa 1.000 seeds completas em cada preset. Um fingerprint de
mundo cobre graph, rooms, transforms, connectors e pontos de corridors, com um
caso dourado compartilhado por Debug/Release.

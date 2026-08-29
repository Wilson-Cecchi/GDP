# World — Room Modules

> **Estado:** contratos de módulo/instância, papéis, temas, zonas, transforms e
> validação offline implementados e validados na Fase 4; seleção procedural
> pertence à Fase 5.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `architecture.md — Room Module`
- `README(6).md — Room Templates`

## Template × instância

`RoomModule` é reutilizável; `Room` é instância concreta.

```cpp
struct RoomModule {
    RoomModuleID id;
    RoomTheme theme;
    GridSize footprint;
    Bounds localBounds;
    std::vector<ConnectorDefinition> connectors;
    std::vector<SpawnZone> enemyZones, lootZones;
    std::vector<PropZone> propZones;
    MeshRecipe geometry;
};
```

Na implementação, IDs são inteiros estáveis, capabilities usam máscara de
`RoomRole` e `RoomInstance` preserva module ID, role, theme, seed, transform,
bounds de mundo e connectors transformados. O módulo industrial fixo é o
primeiro item da biblioteca e passa pelo mesmo gate exigido de módulos futuros.

A Fase 5 acrescenta uma biblioteca inicial validada com variantes Industrial,
Storage, Office, Laboratory, Maintenance, Security e Cafeteria. A seleção filtra
theme e role capability antes de usar o stream `RoomModule` da sala.

## Variedade estrutural implementada

A Fase 20 substitui a base única por 23 módulos. As famílias compactas começam
em `7 × 4 × 9`, as médias variam proporção e altura, e as grandes chegam a
`14 × 6 × 13`. Hub, Arena e Boss selecionam módulos grandes dedicados; Secret
seleciona módulos compactos dedicados.

A implementação cumpre estes contratos:

- `localBounds` e `footprint` do módulo são a única fonte das dimensões;
- renderer e colisão não mantêm medidas fixas paralelas;
- connectors permanecem no perímetro real de cada variante;
- enemy/loot/prop zones são validadas dentro dos novos bounds;
- spacing, overlap e corredores usam os AABBs reais das salas relacionadas;
- seleção por role/theme permanece determinística e evita repetição excessiva;
- toda variante é atravessável nas quatro rotações e em lotes de seeds.

O renderer recebe `worldBounds` de cada instância em vez de reconstruir a caixa
fixa antiga. O caminho principal posiciona cada sala a partir da borda real da
anterior; branches usam as bordas de todos os vizinhos já disponíveis e se
afastam deterministicamente caso encontrem overlap.

A revisão das texturas ocorre somente seguindo o gate de
`visual/TEXTURE_APPROVALS.md`; a Fase 20 não alterou recipes visuais.

## Geometria

A geometria principal é controlada pelo módulo/receita. O procedural varia apenas parâmetros previstos.

## Variações

`Storage_01`, `Storage_02`, etc. Seleção deve evitar repetição excessiva quando houver alternativas.

## Rotação

0/90/180/270°, girando geometry, connectors, zones e semantic surfaces.

## Clearance

Zona de connector não recebe prop, inimigo, loot ou obstáculo.

## Role separado

O mesmo módulo pode servir a `Normal`, `DeadEnd`, `Secret` ou `Arena` se capability permitir.

## Seleção

Filtrar por theme, role capability, connectors, size, depth e histórico recente; só então sortear.

## Validação offline

Todos os módulos: bounds, connectors no perímetro, zones internas, geometry válida e clearance.

O gate atual rejeita ID/nome/footprint/bounds inválidos, ausência de capability
ou connector, IDs repetidos, connector inválido ou fora do perímetro, zona fora
dos bounds e zona que invade o clearance. A validação de `MeshRecipe` será
acoplada quando a geometria procedural substituir a cena fixa do renderer.

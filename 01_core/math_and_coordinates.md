# Core — Matemática e coordenadas

> Na Fase 54, vetores, matrizes, transforms, AABBs e utilitários matemáticos
> passaram para `wkc::core`, sem alterar convenções ou valores.

> **Estado:** contrato matemático básico implementado e validado na Fase 1.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `architecture.md — Room Modules/Connectors`

## Convenção proposta

```text
+X = direita/leste
+Y = cima
+Z = frente/norte
1.0 unidade = 1 metro conceitual
```

OpenGL pode usar conversões internas; a API do jogo mantém uma convenção única.

## Tipos

```cpp
struct Vec2 { float x,y; };
struct Vec3 { float x,y,z; };
struct Transform { Vec3 position; Vec3 rotationDegrees; Vec3 scale{1,1,1}; };
struct GridPos { std::int32_t x,y,z; };
struct Aabb { Vec3 minimum,maximum; };
```

## Grid × mundo

```cpp
Vec3 gridToWorld(GridPos p);
GridPos worldToGrid(Vec3 p);
```

## Rotação de módulos

Primeira versão: `0/90/180/270°`. Geometry, connectors, spawn zones e superfícies semânticas giram juntos.

Na implementação, `QuarterTurn` representa essas quatro rotações e
`rotateQuarterTurns()` gira em torno de `+Y`; yaw positivo leva `+Z` para `+X`.
Conversões grid/mundo aceitam o tamanho da célula, e AABBs validam limites,
testam contenção e podem acompanhar a translação do módulo.

A Fase 91 adicionou `rotationYMatrix()` para composição contínua de yaw em
radianos. A matriz mantém a mesma convenção (`+Z` gira para `+X` com `+90°`) e
retorna identidade para entradas não finitas. A Fase 94 adicionou
`rotationXMatrix()` com o mesmo fallback, permitindo composição genérica de
pitch sem introduzir conceitos de inimigo ou animação na fundação.

## Ângulos

Yaw em +Y; pitch vertical; roll fora do controle padrão. Configuração pode usar graus e matemática interna radianos.

## Precisão

Comparações float após transformação usam epsilon por contexto.

## Testes

Grid→world→grid; 4×90°; connector acompanha módulo; AABB cobre geometria; câmera e movimento concordam sobre frente/direita.

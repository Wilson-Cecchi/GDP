# Renderer — Meshes e geometria procedural

> **Estado:** formato CPU/GPU e Box implementados na Fase 2; receitas compostas,
> transforms, semântica e hashing implementados na Fase 8; Cylinder e montagem
> variável do Worker implementados na Fase 9.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `architecture.md — MeshRecipe/Semantic Regions`
- `Guia_de_texturas_de_Inimigos.md`

## Formato CPU

```cpp
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    std::uint32_t semanticRegion;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    AABB bounds;
};
```

Tangent só entra quando o normal mapping exigir.

## Formato GPU

```cpp
struct GpuMesh { GLuint vao, vbo, ebo; std::uint32_t indexCount; };
```

`ResourceManager` é dono do recurso GPU.

## MeshRecipe

Assets procedurais armazenam receita + parâmetros + seed. `MeshGenerator` converte a receita em `MeshData`.

O contrato atual inclui versão, seed e uma lista ordenada de
`PrimitiveRecipe`. Cada primitiva registra `PrimitiveType`, transform local,
dimensões, `MaterialSlot` e `SemanticRegion`. O hash é sensível a todos esses
campos, inclusive à ordem. Box e Cylinder são geráveis; os demais tipos são
rejeitados como `UnsupportedPrimitive` em vez de produzir fallback oculto.

## Primitivas mínimas

- Box;
- RoundedBox aproximado;
- Cylinder;
- Capsule;
- Sphere;
- Cone;
- ExtrudedProfile.

`makeBoxMesh()` entrega atualmente 24 vértices com hard edges, 36 índices, UVs,
normais, regiões semânticas básicas e bounds exatos. O upload cria VAO/VBO/EBO.
O Cylinder fechado usa 12 segmentos, 54 vértices e 144 índices, com laterais e
tampas corretamente orientadas.
Antes do upload, a validação rejeita mesh vazia, lista de índices incompleta,
índice fora da faixa, valores não finitos e bounds incoerentes.

`generateMesh()` combina múltiplas Boxes/Cylinders, transforma posições e normals,
recalcula bounds e preserva um range de índices por material/semântica. A receita
do Worker da Fase 9 possui oito primitivas variáveis: torso, cabeça, dois braços,
duas pernas, módulo traseiro e ferramenta.

## Regra de silhueta

Se o detalhe altera a silhueta à distância, pertence à mesh. Se é relevo pequeno, pode ir para normal/textura.

## SemanticRegion

A semântica deve sobreviver até materiais/máscaras: `ArmorPlate`, `Fabric`, `Rubber`, `Joint`, `Piston`, `Cable`, `Lens`, `Tool`, `Skin`, `FootContact` etc.

## Submeshes/material slots

Peças multimaterial podem produzir submeshes ou ranges de índices por slot.

## Normals

Blindagem preserva hard edges; curvas podem suavizar. Não suavizar quinas automaticamente.

## Validação

Rejeitar índice inválido, NaN/Inf, bounds inválido, triângulos degenerados relevantes e slots ausentes.

## Debug

Export OBJ temporário e dump de semantic regions.

## Testes

Os testes atuais cobrem Box/Cylinder, combinação, bounds, normals, transformação
Euler, semantic regions, material ranges, entradas inválidas e hash dourado
estável.

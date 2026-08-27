# Enemies — Geração procedural

> **Estado:** Worker completo por partes implementado na Fase 9; Guard e Officer
> completos na Fase 11 sobre a fundação de receita/hash/cache da Fase 8.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Entrada atual

```cpp
WorkerVisual generateWorkerVisual(Seed visualSeed);
GuardVisual generateGuardVisual(Seed visualSeed);
OfficerVisual generateOfficerVisual(Seed visualSeed);
```

`EnemyGenerationRequest` com role e tema continua sendo a expansão prevista
para o elenco completo.

## Pipeline autoritativo

```text
derive sub-seeds
→ BodyProfile
→ compatibility context
→ torso
→ legs/hip
→ shoulders/arms
→ head
→ back modules
→ accessories
→ assembly validation
→ MeshRecipe
→ mesh
→ semantic regions
→ MaterialSet
→ TextureRecipes
→ WearProfile
→ asset hash/cache
→ EnemyVisual
```

## Seleção de peças

Sempre filtrar antes de sortear:

```text
all parts
→ slot
→ role tags
→ body tags
→ socket compatibility
→ required features
→ candidates
→ weighted choice
```

## Falha local

Cada slot usa uma substream própria e seleciona apenas candidatos compatíveis.
Se um slot ficar sem candidato, usa fallback local seguro e incrementa
`fallbackCount`; nunca regenera o mundo. As bibliotecas atuais apresentaram zero
fallbacks nos gates de 1.000 seeds por papel. O Guard usa IDs 1100–1602 e
substreams locais 100–107; o Officer usa IDs 2100–2602 e substreams 200–207,
separados dos IDs 100–602 do Worker.

## Unicidade

O hash canônico inclui seed, perfil, oito IDs de peças, família material,
desgaste e hashes das receitas. A biblioteca não promete unicidade matemática,
mas as primeiras 1.000 seeds de cada papel produziram 3.000 hashes distintos
entre Worker, Guard e Officer.

## Validação

Peças presentes; sockets válidos; pés no plano; sem interseção grave; role legível; material
compatível; asset gerável e determinístico.

## Teste

O gate atual gera 1.000 Workers, valida montagem, mesh, albedo e hash, exige zero
fallbacks e cobre os quatro perfis, quatro estados de desgaste, três famílias
materiais, todas as 18 peças e assimetria esquerda/direita. Uma seed dourada
protege o formato canônico do genoma.

Outro gate gera 1.000 Guards, cobre `Patrol/Tactical/Armored`, quatro estados de
desgaste, `Polymer/PaintedSteel/Polycarbonate`, todas as 18 peças táticas,
assimetria, arma longa obrigatória e zero fallbacks. Hashes dourados e isolação
entre papéis protegem ambos os formatos.

O terceiro gate gera 1.000 Officers, cobre `Field/Command/Veteran`, quatro
estados de desgaste, `Fabric/Polymer/PaintedSteel`, todas as 18 peças, sidearm e
módulo de comando obrigatórios, assimetria e zero fallbacks. Seu hash dourado é
`0x6C70CF22F1AE4F61`; o lote conjunto exige 3.000 hashes sem colisão.

# Enemies — Geração procedural

> **Estado:** Worker completo por partes implementado na Fase 9; Guard, Officer,
> Engineer e IndustrialRobot completos na Fase 11 sobre receita/hash/cache da Fase 8.
> Na Fase 49, esses geradores passaram para `gdp::game::visual` e consomem a
> fundação procedural por nomes qualificados.
> Na Fase 50, essa fundação passou para `wkc::procedural`, preservando todas as
> receitas e hashes visuais.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Entrada atual

```cpp
WorkerVisual generateWorkerVisual(Seed visualSeed);
GuardVisual generateGuardVisual(Seed visualSeed);
OfficerVisual generateOfficerVisual(Seed visualSeed);
EngineerVisual generateEngineerVisual(Seed visualSeed);
IndustrialRobotVisual generateIndustrialRobotVisual(Seed visualSeed);
```

`EnemyGenerationRequest` com role e tema continua sendo a expansão prevista
para o elenco completo.

## Worker: proporções dos membros v1

A proposta [Worker v1](../visual/proposals/worker_limb_variation_v1.md) foi
aprovada em 15/10/2026. Cada braço e perna recebe comprimento e espessura
derivados de sub-seeds versionadas de `visualSeed`; a aparência permanece
estável ao reiniciar ou retomar a mesma seed. A receita mantém a ordem das oito
partes, ancora os pés no piso e ajusta a ferramenta à posição da mão direita.
Somente a apresentação do Worker mudou; colisão, hitboxes, combate, IA, texturas e os
outros quatro papéis preservam seus contratos.

O teste `enemy_generation_systems` valida 1.000 seeds, limites de proporção,
contato das partes, determinismo, hash e zero fallbacks. A seed dourada do
Worker passou a `0x26410B31411A905A` após a versão 2 da receita de malha.
O modo manual `gdp_renderer_gl_tests --worker-limb-capture`, com
`GDP_WORKER_LIMB_CAPTURE_DIR` definido, produz quadros do renderer em MEDIUM e
POTATO, perto e longe, para as seeds 101, 202 e 303, além de caminhada e
ataque em MEDIUM. Essas capturas técnicas não substituem a aceitação visual
em partida.

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
e o Engineer usa IDs 3100–3602 e substreams 300–307, todos separados dos IDs
100–602 do Worker. O IndustrialRobot usa IDs 4100–4602, slots 400–408 e
duas escolhas traseiras independentes; receitas usam substreams 499/401.

## Unicidade

O hash canônico inclui seed, perfil, oito IDs de peças, família material,
desgaste e hashes das receitas. A biblioteca não promete unicidade matemática,
mas as primeiras 1.000 seeds de cada papel produziram 5.000 hashes distintos
entre Worker, Guard, Officer, Engineer e IndustrialRobot.

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

O quarto gate gera 1.000 Engineers, cobre
`Maintenance/Diagnostics/Hazmat`, quatro estados de desgaste,
`Aluminum/Polymer/PaintedSteel`, todas as 18 peças, ferramenta de reparo e
mochila diagnóstica obrigatórias, assimetria e zero fallbacks. Seu hash dourado
é `0x9E65A4A016966EB0`; o lote conjunto exige 4.000 hashes sem colisão.

O quinto gate gera 1.000 IndustrialRobots, cobre `Sentry/Foundry/Siege`, quatro
estados de desgaste, `BareSteel/PaintedSteel/Aluminum`, todas as 18 peças,
torso/núcleo, sensor, braços pesados, dois reatores, arma e emissão obrigatórios.
Seu hash dourado é `0x5D654A79E5B31648`; o lote conjunto exige 5.000 hashes
sem colisão e zero fallbacks.

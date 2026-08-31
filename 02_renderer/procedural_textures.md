# Renderer — Geração procedural de texturas

> **Estado:** infraestrutura de imagem/upload implementada na Fase 2;
> `TextureRecipe`, validação, hash, cache e albedo determinístico implementados na
> Fase 8; paleta/material/desgaste do Worker integrados na Fase 9. Mapas PBR e
> máscaras completas permanecem planejados. Albedos estruturados dos primeiros
> ambientes Industrial, Storage, Combat Room, Cafeteria, Laboratory, Technical
> Area, Security e Central Hub integrados na Fase 10.
> Na Fase 30, contratos e geração reutilizáveis foram isolados no target
> renomeado na Fase 41 para `wkc_forge_procedural`; receitas Worker, fallback
> industrial, luminária e materiais de salas permanecem como conteúdo visual
> do GDP.
> Na Fase 31, esse conteúdo foi agrupado fisicamente em `src/game/visual`, sem
> alterar namespaces, receitas ou hashes.
> Na Fase 42, o `AssetCache` genérico passou para `src/resources` e para o target
> header-only `wkc_forge_resources`, preservando API e métricas.
> Esses albedos são baseline técnico. Qualquer revisão artística deve cumprir o
> gate pré-implementação de `visual/TEXTURE_APPROVALS.md`.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `architecture.md — texturas procedurais`
- `Guia_de_texturas_de_Inimigos.md`
- `Guia_de_texturas(1).md`

## Objetivo

Converter:

```text
RECIPE + SEED + MATERIAL + SEMÂNTICA
```

em:

```text
ALBEDO + ROUGHNESS + METALLIC + NORMAL + EMISSIVE
```

sem depender de uma imagem final pronta.

## Gate de revisão visual

Antes de alterar uma recipe existente por motivo artístico, preparar uma
prancha versionada da unidade visual com no mínimo três seeds, detalhe próximo,
enquadramento de gameplay e iluminação neutra. Até a resposta explícita
`APROVADA PARA IMPLEMENTAÇÃO`, a proposta não pode substituir a recipe atual.

A aprovação se aplica à recipe/superfície citada, não a todas as variantes do
tema. Depois da implementação, capturas in-engine precisam demonstrar aderência
à prancha aprovada sem dispensar os gates técnicos abaixo.

A Fase 2 estabeleceu `ImageData` RGBA validado, filtros nearest, repetição,
mipmap e lifetime RAII. A Fase 8 acrescentou a geração de albedo por receita com
macro/micro variação, dirt, wear e oxidação compatível com o material. Scratch,
emissive, normal, roughness e metallic já participam ou estão reservados no
contrato, mas ainda não possuem todos os mapas autoritativos descritos abaixo.
A Fase 9 deriva a receita do material e desgaste de cada `WorkerVisual`; renderer
e `AssetCache` preparam cada variante por `visualSeed` antes do draw.
A API genérica `AssetRecipe` não oferece construtores nomeados por Worker ou
ambiente. Esses construtores vivem em `game/visual/VisualAssets` e
`gdp_visual_content`, enquanto `Texture` mantém apenas `ImageData`, validação e
upload reutilizáveis.
A Fase 10 acrescenta receitas específicas de superfície: placas e juntas não
dependem de noise puro; parede reserva amarelo/preto apenas na base, piso recebe
desgaste direcional no centro do fluxo e teto distribui luminárias lineares.
Storage usa manchas grandes por células, oxidação restrita às juntas/rodapé da
parede, marca logística curta, arrastos e rachadura rara no concreto e fixtures
espaçadas com falha por painel. Esses detalhes continuam subordinados ao budget
global do tema.
Combat Room usa placas blindadas verde-oliva, impactos circulares limitados,
demarcação contínua de arena, desgaste de passagem, vigas e luzes espaçadas.
Cafeteria usa painéis claros, faixa vermelha alinhada, lajes laváveis com
circulação/contatos de mobiliário e luzes largas regulares.
Laboratory usa microvariação mínima, painel científico organizado, piso técnico
com tampa/travas e mancha de contenção localizada, além de luzes por módulo.
Technical Area compõe tubos em três tons, válvula/caixa conectadas, grade, óleo
limitado e categorias mutuamente exclusivas de teto.
Security compõe um monitor abstrato, indicadores agrupados, alerta confinado,
tampa de acesso e luzes regulares com sensor isolado.
Central Hub mantém painéis amplos e neutros, uma placa ciano, uma ou duas linhas
de navegação contínuas e luminárias coerentes com o eixo compartilhado.

## Entradas

`TextureRecipe` carrega versão, seed, família, cores, intensidade macro/micro,
wear, dirt, scratch, oxidation, emissive, semantic region e resolução
power-of-two entre 8 e 1.024.

## Pipeline autoritativo

```text
1 imagens base
2 macroestrutura
3 máscaras semânticas
4 microestrutura
5 wear mask
6 dirt mask
7 detalhes especiais
8 albedo
9 roughness
10 metallic
11 height auxiliar
12 height→normal
13 emissive
14 validação
15 hash/cache
16 upload GPU
```

## Macroestrutura

Usar funções com intenção: `drawPanel`, `drawSeam`, `drawBolt`, `drawStripe`, `drawWeld`, `drawFabricSeam`. Noise puro não substitui design.

## Microestrutura

Baixa amplitude: value noise, grão, trama, brushed metal e microrriscos.

## Máscaras

```text
EDGE      tinta lascada/polimento
JOINT     graxa/óleo
CONTACT   poeira/abrasão
CAVITY    sujeira/oxidação leve
EXPOSED   material base exposto
HEAT      fuligem/descoloração/emissivo
```

A máscara decide **onde**; noise local decide **como varia**.

## Riscos

1. quantidade limitada por material;
2. origem em região permitida;
3. comprimento/direção por seed;
4. rasterização com largura pequena;
5. atenuação das pontas;
6. rejeição se estourar orçamento visual.

## Oxidação

Aplicar principalmente a metal ferroso exposto e cavidades justificáveis. Proibida em lente, tecido, borracha e pele.

## Dirt types

`dust`, `oil`, `soot`, `organicStain`, cada um com regiões compatíveis.

## Height → normal

```text
dx = h(x+1,y)-h(x-1,y)
dy = h(x,y+1)-h(x,y-1)
N = normalize(-dx*strength, -dy*strength, 1)
```

## Mapas

- Albedo: cor, variação, marcas; não bakear iluminação direcional forte.
- Roughness: material/estado.
- Metallic: metal exposto vs materiais dielétricos.
- Normal: relevo pequeno; nunca substituir geometria que muda silhueta.
- Emissive: somente função visual clara.

## Resolução

Primeira faixa: 64/128/256. Regras visuais não podem depender de uma resolução fixa.

## UV

Box, cylindrical, planar, recipe UV e triplanar quando necessário. Evitar detalhe importante atravessando seams.

## Salas

Composição global vem antes do detalhe local. A sala define budgets; a superfície individual não sorteia highlights ilimitados.

## Cache key

Recipe version + seed + material + parâmetros + resolução.

## Falha/debug

Falha gera log + textura de erro. Debug pode salvar albedo/roughness/metallic/normal/emissive e todas as máscaras intermediárias.

## Testes

Implementados: mesma recipe gera os mesmos pixels, seed diferente altera pixels,
ranges válidos, ferrugem nunca em Fabric, imagem opaca válida, hashes dourados,
1.000 recipes de assets, 1.000 Workers, 1.000 Guards, 1.000 Officers, 1.000 Engineers completos e 8.000 conjuntos
de sala
(1.000 por perfil Industrial, Storage, Combat, Cafeteria, Laboratory, Technical,
Security e Central Hub) sem falha. Normal sem NaN e mapas emissivos
dedicados entram com os mapas completos; as primeiras luminárias usam máscara
por luminância no shader.

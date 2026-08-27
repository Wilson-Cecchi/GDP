# Enemies — Materiais, texturas e desgaste

> **Estado:** material, paleta e desgaste do Worker implementados na Fase 9 e do
> Guard e Officer na Fase 11; conjuntos multimaterial e mapas adicionais permanecem
> planejados.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Material set

O primeiro Worker sorteia `PaintedSteel`, `BareSteel` ou `Aluminum`, uma paleta
primária e uma secundária derivada. Os material slots e regiões semânticas ficam
preservados na mesh para a futura composição multimaterial. Fabric, Rubber,
Polymer, Polycarbonate, OrganicSkin e CorruptedOrganic continuam no contrato.

O Guard restringe a seleção a `Polymer`, `PaintedSteel` e `Polycarbonate`, com
paletas dessaturadas azul, oliva, marrom ou grafite. Isso mantém leitura tática
e separa seus substreams de material dos do Worker.

O Officer usa `Fabric`, `Polymer` e `PaintedSteel`, com paletas organizadas em
azul-marinho, grafite, bordô ou oliva. Seu desgaste tende a ser menor que o do
Guard e deriva da substream de papel 200.

## Conservação

Estados implementados: `Clean`, `Used`, `Worn` e `Damaged`. Cada um delimita
faixas de wear e dirt; scratch deriva do wear e oxidation deriva do dirt apenas
quando a família permite. `SeverelyDamaged` e `Corrupted` ficam para roles
futuros.

## Semântica

- junta → graxa/óleo;
- pistão → risco longitudinal;
- placa externa → impacto/tinta gasta;
- tecido → abrasão/mancha;
- lente → risco/rachadura;
- pé → poeira;
- região térmica → fuligem/descoloração.

## Regra

A variação de cor nunca troca a natureza física do material. Uma mangueira pode mudar de
cor, mas permanece Rubber.

## Pipeline

`EnemyMaterialSeed → palette/family → WearProfile → TextureRecipe → albedo → cache → GPU`.

Material e desgaste usam domínios de seed independentes. A primeira integração
aplica um albedo de 64×64 ao inimigo inteiro; os ranges semânticos já permitem
evoluir para conjuntos por peça sem alterar o genoma.

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
contato das partes, determinismo, hash e zero fallbacks. A versão 4 da receita
de malha mantém as oito partes e a variação de membros, com hash dourado
`0x9A19153D1316F3F5` para o Worker com serra.
O modo manual `gdp_renderer_gl_tests --worker-limb-capture`, com
`GDP_WORKER_LIMB_CAPTURE_DIR` definido, produz quadros do renderer em MEDIUM e
POTATO, perto e longe, para as seeds 101, 202 e 303, além de caminhada e
ataque em MEDIUM. Essas capturas técnicas não substituem a aceitação visual
em partida.

## Worker: modelo e material v2

Aprovado em 16/10/2026: [conceito laranja usado](../visual/proposals/worker_model_material_v2_orange_used.png).
O gerador preserva IDs, sockets, pivôs e as oito partes animadas da v1. O
torso é mais largo e menos profundo, a cabeça cilíndrica é mais larga, e a
ferramenta v2 era um bloco retangular pesado ligado à mão. Doze grupos de
detalhes determinísticos seguem as poses dos pais: painel laranja amplo com
borda fina de aço e quatro fixadores; faixa escura cilíndrica ao redor da
cabeça e abertura âmbar; separações escuras no pescoço e na cintura; duas
ombreiras inclinadas e luvas laranjas separadas por ombros e cotovelos
grafite; duas canelas e botas laranjas sobre quadril e joelhos grafite. Cada
braço e perna tem três cabeças de parafuso escuras, cilíndricas e parcialmente
embutidas (duas na placa, uma na luva ou bota), agrupadas por parte animada. A
ferramenta e a mochila usam grafite. A pintura laranja usa o bitmap
`assets/visual/worker_painted_steel_v2.png`, derivado do conceito aprovado;
o painel frontal amostra diretamente a região de pintura da prancha em
`assets/visual/worker_front_panel_source_v2.png`. As demais peças usam a
textura neutra com tintas por parte. O shader acrescenta desgaste nas arestas
laranjas e escurece as bordas para separar as placas. A seed mantém quatro
estados de desgaste.

`WorkerVisual::partTints` traz oito tintas, na ordem das partes base.
`WorkerVisual::details` traz receitas em coordenadas do Worker montado, com
`parentPartIndex` entre 0 e 7; o renderer aplica a pose do pai. O custo dessa
etapa era de 8 bases + 12 grupos filhos, com 41 primitivas e até 20 draws por
Worker visível antes de qualquer agrupamento. A aprovação do conceito e
os testes de receita não
substituem capturas nativas em POTATO/MEDIUM, medição de quadro e aceitação
visual em partida.

## Worker: serra circular visual v3

O usuário gostou da troca da ferramenta do Worker pela serra circular da
[referência separada](../visual/Proposta_visual_separada/Worker.md). A base
retangular da ferramenta foi encurtada para alojar um disco de aço escuro com
12 dentes, parcialmente protegido por uma carcaça laranja. A face do disco
aponta para o lado do Worker, como na prancha; de frente, aparece o perfil
dentado. A serra segue a parte animada da ferramenta, enquanto o disco gira em
torno do próprio eixo no renderer. Os detalhes 12 e 13 pertencem à parte 7;
o detalhe 13 mantém o disco como primeira primitiva para definir o pivô.

A receita mantém as oito partes base e acrescenta dois grupos, totalizando
57 primitivas e 22 draws por Worker visível, além do draw do halo do olho. A
geração continua determinística em 1.000 seeds sem fallback; a validação
confere encaixe, dentes, limites e folga do chão. Alcance, dano, colisão e IA
não mudaram. As capturas técnicas frontal/lateral e MEDIUM/POTATO estão em
`visual/validation/worker_saw_v2/`; a aceitação visual em partida está pendente.

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

O hash canônico do Worker inclui seed, perfil, oito IDs de peças, família
material, desgaste, tintas e hashes das oito peças e oito detalhes. A biblioteca
não promete unicidade matemática,
mas as primeiras 1.000 seeds de cada papel produziram 5.000 hashes distintos
entre Worker, Guard, Officer, Engineer e IndustrialRobot.

## Validação

Peças presentes; sockets válidos; pés no plano; sem interseção grave; role legível; material
compatível; asset gerável e determinístico.

## Teste

O gate atual gera 1.000 Workers, valida montagem, mesh, albedo e hash, exige zero
fallbacks e cobre os quatro perfis, quatro estados de desgaste, aço pintado,
todas as 18 peças e assimetria esquerda/direita. Uma seed dourada
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

# Armas em primeira pessoa — proposta visual v1

```text
estado: IMPLEMENTADA EM VALIDAÇÃO (variante Used)
unidade visual: Weapon/first-person presentation
versão: v1
data: 2026-09-26
pranchas: weapon_first_person_v1_maintained.png
          weapon_first_person_v1_used.png
          weapon_first_person_v1_worn.png
resolução: 1536 × 1024 RGB por prancha
geração: image_gen, conceito visual; não são capturas do jogo nem assets prontos
SHA-256 Maintained: f19d5486c5d4619482f9081116fc44ed81e54c2341dc7b0cd2a47ac7c27dbd41
SHA-256 Used: bf1bf4f076915b4fda0329b0b177eb316e8760bc5403f2d604ad4a8199e4eb22
SHA-256 Worn: adf13cbb1f052a3b7d8e1a82b4a98771963daa33e4f81c9071d8aaa214be7499
decisão do usuário: "aprovada para implementação da usada"
capturas in-engine: ../validation/weapon_first_person_v1_used_pistol_gameplay.png;
                   ../validation/weapon_first_person_v1_used_pistol_close.png;
                   ../validation/weapon_first_person_v1_used_carbine_gameplay.png;
                   ../validation/weapon_first_person_v1_used_carbine_close.png;
                   ../validation/weapon_first_person_v1_used_knife_gameplay.png;
                   ../validation/weapon_first_person_v1_used_knife_close.png;
                   ../validation/weapon_first_person_v1_used_carbine_potato.png;
                   ../validation/weapon_first_person_v1_used_comparison.png;
                   ../validation/weapon_first_person_v1_used_pose_correction_comparison.png
```

## Objetivo

Substituir a aparência de blocos simples do rifle, da pistola e da faca por
silhuetas mecânicas convincentes, escala ergonômica e materiais metálicos
legíveis em primeira pessoa. O tom continua industrial e sério, sem marcas de
armas reais nem grandes faixas emissivas.

As três colunas de cada prancha mostram, da esquerda para a direita, **pistola,
rifle e faca**. A linha superior é uma vista próxima para avaliar forma e
material; a inferior simula a leitura durante o gameplay. As cenas inferiores
são ilustrações de referência, não capturas da build atual. Mãos e corredor
servem apenas para escala e contexto, não são parte da unidade aprovada.

## Direção de forma e material

- Pistola: slide, abertura de ejeção, boca do cano, miras, guarda-mato,
  carregador e empunhadura diferenciados, com volumes chanfrados.
- Rifle curto: coronha, receiver, handguard, cano, carregador, empunhadura e
  miras com junções e proporções funcionais; evitar tubo/caixa monolíticos.
- Faca: lâmina de aço escovado com espessura e bisel perceptíveis, guarda
  discreta, construção full-tang e cabo texturizado.
- Aço grafite e gunmetal com reflexo metálico contido; polímero preto fosco
  em cabos e guardas; ciano somente em minúsculos pontos de leitura.
- Detalhes concentrados nas áreas próximas à câmera: bevels, juntas,
  fixadores, serrilhas e variação de rugosidade. Nada de sujeira aleatória
  uniforme, neon decorativo ou ferrugem generalizada.

## Variantes de condição

1. [Maintained](weapon_first_person_v1_maintained.png): revestimento íntegro,
   microtextura de fabricação e metal limpo.
2. [Used](weapon_first_person_v1_used.png): desgaste moderado nos contatos,
   riscos finos e graxa/poeira discretas em recessos.
3. [Worn](weapon_first_person_v1_worn.png): abrasão mais visível, sem peças
   quebradas, mantendo a arma operacional e a maior parte do revestimento.

A mesma família de silhuetas aparece nas três condições. As imagens geradas
não garantem correspondência pixel a pixel entre variantes; a implementação
deverá usar uma única geometria por arma e variar apenas o acabamento. A
condição-base sugerida para o jogo é `Used`.

## Contrato para implementação após aprovação

Remodelar somente os viewmodels em primeira pessoa. Construir geometrias
otimizadas e reutilizar materiais sempre que possível; não inserir as pranchas
como texturas de tela. Ajustar enquadramento e pontos de muzzle flash sem
mudar origem de tiro, dano, alcance, slots, troca de arma ou animações
funcionais. Verificar silhueta também em `POTATO` e `LOW`, preservando a
legibilidade e o orçamento de GPU do hardware antigo. Os detalhes das
pranchas são direção artística, não promessa de malha cinematográfica.

Após a implementação: capturar cada arma em jogo, perto e em combate;
comparar com a direção aprovada; rodar regressões de renderer/gameplay e
medir o custo dos perfis gráficos antes de pedir aceitação visual final.

Uma aprovação desta v1 cobre apenas `Weapon/first-person presentation` para
rifle, pistola e faca. Não aprova braços/mãos novos, HUD, inimigos, armas de
terceira pessoa, efeitos de disparo ou mudanças na lógica de combate.

## Implementação da variante Used

As três armas agora usam malhas procedurais próprias, agrupadas por cinco
materiais compartilhados para manter poucas chamadas de desenho. Slide e
receiver possuem chanfros reais; miras, janela de ejeção, estrias, rail,
carregador, guarda-mato e lâmina com seção em losango são peças geométricas.
Aço revestido, aço aparente e polímero usam albedo, normal e RMA independentes.
Os mapas principais são `512×512`; recessos e miras usam `128×128`. Uma luz de
preenchimento apenas no viewmodel mantém as faces metálicas legíveis quando a
iluminação da sala vem de trás, sem alterar os ambientes.

Na [comparação in-engine](../validation/weapon_first_person_v1_used_comparison.png),
as colunas são pistola, rifle e faca; a linha superior mostra gameplay e a
inferior, um campo de visão mais estreito para inspeção próxima.

As capturas acima vêm do renderer OpenGL com cena de corredor fixa, não de uma
partida controlada manualmente. A prancha conceitual tem detalhamento mais
alto e mãos ilustrativas que não foram incorporadas à build; a validação
visual final deve considerar as capturas reais e um teste jogando.

Na cena de captura, foram medidos `58` draw calls para pistola/rifle e `57`
para faca, com `1256`, `1524` e `992` triângulos respectivamente (incluindo
todo o corredor, não apenas a arma). Esses números não são um benchmark de FPS.
Os perfis leves preservam as mesmas silhuetas e desligam a resposta detalhada
de normal/RMA conforme a configuração já existente.

Estado final ainda pendente de aceitação humana: `IMPLEMENTADA EM VALIDAÇÃO`.

### Correção após o primeiro feedback

O usuário identificou a empunhadura invertida e a área excessiva das armas na
tela. A pistola e o rifle agora inclinam a base do cabo para trás, não para o
cano; um teste de geometria verifica essa relação. A apresentação foi reduzida
e reposicionada no canto inferior direito sem mover a mira lógica ou alterar
alcance, dano e animações. A faca também recebeu enquadramento menor.

Na [comparação antes/depois](../validation/weapon_first_person_v1_used_pose_correction_comparison.png),
as colunas são pistola, rifle e faca; a primeira linha é a captura anterior e
a segunda é a correção, ambas em 1280×720. Comparando os pixels alterados em
relação à mesma cena sem arma, com tolerância de 2% por canal, a área caiu de
`64039` para `39046` pixels na pistola (~39%), `97401` para `48656` no rifle
(~50%) e `44465` para `21912` na faca (~51%). Essa é uma medida de ocupação
visual naquela cena, não um benchmark de GPU nem garantia para todo FOV.

## Prompts usados

### Maintained

```text
Create a landscape 3:2 production concept approval board for a cold industrial first-person shooter game, visual unit WEAPON / FIRST PERSON PRESENTATION, proposal v1. This image is Variant A: MAINTAINED, clean but believable service condition. One cohesive design family of exactly three practical fictional weapons: a compact semi-automatic pistol, a short carbine rifle, and a fixed-blade tactical utility knife. No real-world trademarks, markings or brands. Layout as a clear 3-column comparison: each weapon shown in a large close three-quarter studio view in top row and a small accurate first-person gameplay view in bottom row, held toward the lower right, against a dark cold steel corridor with pale gray-blue wall panels, neutral ceiling and separate ceiling fixture; cool moody but readable lighting. Maintain perfectly consistent design and proportions between the two views. The pistol must have slide, functional-looking ejection port, barrel opening, squared trigger guard, distinct grip and sights. The carbine must have stock, receiver, handguard, barrel and muzzle, magazine, grip, sights and restrained mechanical seams. The knife must have a practical clip-point brushed steel blade with visible thickness and bevel, small guard, full-tang construction and textured dark grip. Credible human scale, ergonomic proportions, controlled bevels, real mechanical layering, dark graphite and brushed gunmetal steel, matte charcoal polymer, small restrained cyan status detail only. HD PBR materials with convincing metallic highlights, roughness differences, tiny fasteners and manufacturing microtexture; no giant colored emissive slabs. Real-time game-asset concept feasible as optimized modular low-mid-poly 3D geometry and reusable materials in OpenGL 3.3, not a photobash. Clean neutral review lighting in studio views, no motion blur, no HUD, no people, no hands obscuring the shape, no gore, no extra weapons, no text or labels, no watermark. The bottom game previews should show a believable first-person angle, not an isometric third-person pose. Distinct metal versus polymer response should be easy to judge.
```

### Used

```text
Edit the Maintained board into Variant B: USED. Preserve the exact 3-column and 2-row composition, camera views, same pistol/carbine/knife silhouettes and mechanical features, same corridor and neutral studio lighting. Change only the finish to credible moderate service wear: satin-polished contact edges on gunmetal parts, subtle hairline scratches on slide and receiver, lightly worn black polymer grip texture, tiny localized oil and dust in recesses, restrained rubbed bevels, minor scuffs on the brushed steel knife, without rusty or damaged functionality. Keep physically believable metallic reflections and different roughness between steel and polymer. No changes to weapon design, no extra accessories, no logos, no text, no watermark, no HUD. This is a game art concept preview, not a final game asset.
```

### Worn

```text
Edit the Maintained board into Variant C: WORN BUT FUNCTIONAL. Preserve the exact 3-column and 2-row composition, camera views, same pistol/carbine/knife silhouettes and mechanical features, same corridor and neutral studio lighting. Change only the finish to credible accumulated service wear: more pronounced but localized rubbed coating on steel edges and grip contact points, fine directional scratches and scuffs on the pistol slide, carbine receiver/handguard, and knife blade, embedded grime only around seams and fasteners, slightly dulled polymer texture. Every weapon must remain mechanically intact, maintained and operable; preserve most dark graphite coating, no blanket rust, no orange dirt, no broken pieces, no glowing slashes. Keep physically believable metallic highlights and roughness separation, crisp functional details. No changes to weapon design, no extra accessories, no logos, no text, no watermark, no HUD. This is a game art concept preview, not a final game asset.
```

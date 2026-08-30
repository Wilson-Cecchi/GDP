# Industrial Corridor / Wall — proposta visual v1

```text
estado: REVISÃO SOLICITADA APÓS CAPTURAS IN-ENGINE
unidade visual: Industrial Corridor/Wall
versão: v1
data: 2026-08-30
prancha: industrial_corridor_wall_proposal_v1.png
resolução: 1536 × 1024 RGB
SHA-256: d250008f97ad327a567a5a0c6c45b05668dab54579407cf33c1527e0ea99ab22
seeds: 2 Maintained; 3 Used; 1 Worn
geração: ferramenta built-in image_gen
referência: ../reference_sheet_rooms_enemies.png, usada somente para linguagem,
            atmosfera, paleta e apresentação
decisão do usuário: "aprovado"
capturas in-engine: ../validation/industrial_corridor_wall_v1_maintained_seed2.png;
                    ../validation/industrial_corridor_wall_v1_used_seed3.png;
                    ../validation/industrial_corridor_wall_v1_worn_seed1.png
resultado: implementação considerada simples e de baixa qualidade; solicitado
           material mais complexo e com qualidade ao menos HD
```

## Objetivo

Estabelecer a parede-base dos corredores industriais com leitura clara em vista
próxima e durante deslocamento. A estrutura deve parecer modular e procedural,
sem depender de ruído aleatório como detalhe principal.

## Direção proposta

- aço pintado cinza frio, roughness média e juntas recuadas escuras;
- painéis amplos, bordas estruturais, parafusos restritos e escovado vertical;
- metal exposto apenas em desgaste de borda justificável;
- sujeira concentrada em juntas e zonas inferiores de contato;
- faixa âmbar/preta contínua somente no rodapé funcional;
- iluminação neutra, sem sombras direcionais fortes gravadas no albedo;
- densidade de microdetalhe baixa o bastante para preservar a leitura distante.

Paleta proposta: `#555A5C`, `#303638`, `#151A1C`, `#D49A00`,
`#8B9091`, `#49382B`.

## Variantes mostradas

- seed `2`, `Maintained`: painéis limpos, estrutura nítida e desgaste mínimo;
- seed `3`, `Used`: abrasão moderada abaixo da cintura e graxa leve nas juntas;
- seed `1`, `Worn`: chips e sujeira mais fortes, preservando a maior parte da
  tinta e a estrutura dos painéis.

## Escopo da eventual aprovação

Uma resposta `APROVADA PARA IMPLEMENTAÇÃO` autoriza somente a direção visual
desta versão para `Industrial Corridor/Wall`. Não autoriza Floor, Ceiling,
Door, outros ambientes, nem implementação divergente da prancha. Até essa
resposta, nenhuma recipe, shader, paleta ou textura do jogo deve mudar.

## Prompt final

```text
Use case: stylized-concept
Asset type: versioned visual approval board for a procedural retro-industrial FPS wall material
Primary request: Create a polished landscape concept sheet for exactly one visual unit, Industrial Corridor / Wall, proposal version V1. This is a direction board for human approval, not a final texture atlas.
Input images: Image 1 is a style, mood, palette, and presentation reference only; do not copy its rooms, characters, exact layout, or text.
Scene/backdrop: dark charcoal professional concept-board background with thin cool-gray dividers, consistent with the GDP visual reference language.
Subject: modular painted-steel corridor wall surface designed for procedural generation. Broad readable panels, recessed seams, restrained bolts, subtle vertical brushing, localized edge chips, grime concentrated in joints and lower contact zones. A continuous functional amber-and-black safety strip exists only along the wall base near the floor.
Composition/framing: landscape approval board. Header at top. Top row has three equally sized orthographic flat wall material variants under neutral light: seed 2 maintained, seed 3 used, seed 1 worn. Bottom left shows a macro close-up of one seam, rim, paint chip, exposed steel, and joint grime. Bottom right shows the same material in a long first-person gameplay corridor perspective under neutral white overhead lights, with no HUD. Include a small six-swatch palette strip.
Lighting/mood: neutral diffuse material-review light in samples; gameplay view remains legible, moody, and cool without crushing blacks or dramatic baked directional lighting.
Color palette: cold medium gray #555A5C, dark steel #303638, deep joint #151A1C, warning amber #D49A00, exposed steel #8B9091, restrained grime brown #49382B.
Materials/textures: painted steel with medium roughness, dark recessed joints, exposed metal only at justified edge wear. Maintained variant has crisp panels and minimal wear. Used variant has moderate scuffs below waist height and light joint grease. Worn variant has heavier edge chips and grime but preserves most painted area and clear panel structure.
Text (verbatim): "GDP — INDUSTRIAL CORRIDOR / WALL — PROPOSTA V1"; "SEED 2 — MAINTAINED"; "SEED 3 — USED"; "SEED 1 — WORN"; "CLOSE MATERIAL DETAIL"; "GAMEPLAY VIEW"; "AGUARDANDO APROVAÇÃO"
Typography: compact uppercase technical sans-serif, high legibility, no other words.
Constraints: exactly three procedural variants; clearly show increasing wear; wall only; base safety stripe only; modular and tileable visual logic; readable at close range and gameplay distance; proposal must look feasible for a low-resolution procedural albedo plus future roughness/normal masks.
Avoid: enemies, people, weapons, HUD, doors as focal objects, pipes covering the wall, random noise confetti, blanket rust, orange/brown overall cast, universal hazard borders, excessive microdetail, photoreal grime overload, logos, watermark, extra text, misspelled labels.
```

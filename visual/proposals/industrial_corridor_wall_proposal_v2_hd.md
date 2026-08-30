# Industrial Corridor / Wall — proposta v2 HD

```text
estado: IMPLEMENTADA EM VALIDAÇÃO
unidade visual: Industrial Corridor/Wall
versão: v2 HD
data: 2026-08-30
prancha: industrial_corridor_wall_proposal_v2_hd.png
resolução da prancha: 1536 × 1024 RGB
resolução-alvo do material: 1024 × 1024 por mapa
SHA-256: d88e4d45cc862be28083e7c5849f13deb83e0ca53083a360b4edc59f22a92ddf
seeds: 2 Maintained; 3 Used; 1 Worn
geração: ferramenta built-in image_gen; edição pontual de tipografia
referências: industrial_corridor_wall_proposal_v1.png como base de comparação;
             ../reference_sheet_rooms_enemies.png somente para linguagem e atmosfera
decisão do usuário: "se a textura for parecida com a que vc me mostrou pode prosseguir"
capturas in-engine: ../validation/industrial_corridor_wall_v2_hd_maintained_seed2_gameplay.png;
                    ../validation/industrial_corridor_wall_v2_hd_maintained_seed2_close.png;
                    ../validation/industrial_corridor_wall_v2_hd_used_seed3_gameplay.png;
                    ../validation/industrial_corridor_wall_v2_hd_used_seed3_close.png;
                    ../validation/industrial_corridor_wall_v2_hd_worn_seed1_gameplay.png;
                    ../validation/industrial_corridor_wall_v2_hd_worn_seed1_close.png
```

## Motivo da revisão

A v1 foi recusada depois das capturas in-engine porque a geração `64×64`, o
filtro nearest e um UV único esticado por vão produziram blocos grandes, marcas
simples e uma parede visualmente plana. A v2 não tenta corrigir isso apenas com
mais ruído: ela muda o contrato técnico do material.

## Direção proposta

- painéis grandes de aço galvanizado pintado com moldura chanfrada rasa;
- juntas realmente recuadas por normal map, gasket escuro e bevel legível;
- parafusos escareados, nervuras prensadas e poucas placas de serviço;
- três escalas de detalhe: variação ampla, desgaste intermediário e microtextura;
- orange peel da tinta, grão do metal, riscos finos e poeira nos recessos;
- metal exposto apenas em bordas justificadas e sem riscos brancos gigantes;
- sujeira concentrada nas juntas, contato inferior e placas de manutenção;
- faixa âmbar/preta somente no rodapé, com relevo e desgaste próprios.

## Contrato técnico proposto

```text
resolução-base: 1024 × 1024
densidade: 256 texels por metro; tile modular cobre aproximadamente 4 m
canais: Albedo RGB; Normal tangent-space; Roughness/Metalness/AO empacotados
filtragem: trilinear com mipmaps; anisotropia quando disponível
UV: repetição por dimensão física, nunca uma textura esticada por corredor
shader: resposta de normal, roughness e metalness em OpenGL 3.3
variação: base HD compartilhada + máscara/offset determinístico leve por sala
```

A base compartilhada evita multiplicar três mapas de 1024 por cada sala. Seed,
condição e pequenos masks continuam variando o resultado sem transformar a
qualidade HD em consumo ilimitado de VRAM.

## Variantes mostradas

- seed `2`, `Maintained`: coating íntegro, microtextura de fabricação e pouca sujeira;
- seed `3`, `Used`: polimento de contato, riscos moderados e graxa leve nas juntas;
- seed `1`, `Worn`: abrasão e sujeira mais fortes, preservando a maior parte da tinta.

## Escopo da eventual aprovação

Uma aprovação autoriza somente a direção e o contrato técnico de
`Industrial Corridor/Wall v2 HD`. Não autoriza Floor, Ceiling, Door, outros
ambientes, inimigos, arma, HUD nem mudança global do filtro das texturas.

## Prompt final

```text
Use case: stylized-concept
Asset type: versioned HD material approval board for a procedural OpenGL retro-industrial FPS
Primary request: Create proposal V2 HD for exactly one visual unit, Industrial Corridor / Wall. Replace the simple low-resolution result with a production-grade, high-definition layered material direction that is visibly feasible as a 1024 x 1024 procedural PBR-style material set.
Input images: Image 1 is the previous V1 proposal; preserve its professional approval-board hierarchy, the three-condition comparison, close material inspection, and gameplay view, but substantially upgrade material complexity and clarity. Image 2 is a general style, atmosphere, and palette reference only; do not copy its exact rooms, characters, text, or layout.
Scene/backdrop: dark charcoal technical concept-board background with thin cool-gray dividers.
Subject: modular cold-gray industrial wall assembled from large painted galvanized-steel panels. Each panel has a shallow chamfered frame, physically recessed gasket seams, countersunk fasteners, subtle pressed ribs, a few small service access plates, restrained serial stencil marks, localized weld discoloration, and believable manufacturing variation. Surface detail uses layered scale: broad tonal variation, mid-scale paint wear and joint grime, then fine orange-peel paint roughness, brushed-metal grain, hairline scratches, tiny edge chips, dust caught in recesses, and lower-wall contact grime. Exposed metal appears only at justified edges. A continuous amber-and-black safety strip exists only at the base and has its own scuffed paint and raised edge.
Style/medium: premium real-time game material concept, high-definition stylized realism, sharp physically based 3D material rendering, not photobashed concept art.
Composition/framing: retain a landscape approval-board layout. Top row shows exactly three equally sized orthographic wall variants: seed 2 Maintained, seed 3 Used, seed 1 Worn. Bottom includes a large macro close-up where fine paint roughness, bevel depth, seam gasket, fastener, scratch layers, and edge chips are unmistakably visible; one compact technical strip showing ALBEDO, NORMAL, and ROUGHNESS / METALNESS map previews; and a first-person GAMEPLAY VIEW of a long corridor under neutral lighting with no HUD. Make the quality increase from V1 immediately obvious at both close and gameplay distance.
Lighting/mood: neutral material-review lighting in samples with grazing highlights that reveal normal and roughness response; corridor remains cool, moody, and fully legible without crushed blacks.
Color palette: cold medium steel gray #555A5C, dark steel #303638, deep gasket joint #151A1C, warning amber #D49A00, exposed steel #8B9091, restrained grime brown #49382B, plus subtle cool highlight variation.
Materials/textures: target 1024 x 1024 per material map, consistent texel density, trilinear presentation, layered albedo plus tangent-space normal plus packed roughness/metalness. Maintained has pristine coating with faint manufacturing detail. Used has moderate lower scuffs, hand-contact polish, and light joint grease. Worn has heavier edge wear, abrasion, and embedded grime while preserving most paint and panel structure.
Text: exact technical labels shown on the saved board; no other words.
Constraints: exactly three procedural variants; unmistakable increasing wear; wall only; safety strip only at the base; high-frequency detail must remain controlled and physically justified; modular and tileable construction; no baked directional shadows in albedo; visual target must be feasible in OpenGL 3.3.
Avoid: low-resolution pixel art, blurry or blocky enlargement, flat albedo-only appearance, random noise confetti, oversized bright scratches, blanket rust, excessive orange/brown cast, universal hazard borders, pipes covering the wall, enemies, people, weapons, HUD, doors as focal objects, logos, watermark, extra labels, misspelled labels.
```

# Industrial Corridor / Ceiling — proposta v1 HD

```text
estado: ACEITA NO JOGO
unidade visual: Industrial Corridor/Ceiling
versão: v1 HD
data: 2026-08-30
prancha: industrial_corridor_ceiling_proposal_v1_hd.png
resolução da prancha: 1672 × 941 RGB
resolução-alvo do material: 1024 × 1024 por mapa
SHA-256: 50efa5ef893878f3a7d3264d4094bf1e8264354ecc7a6250b8f0b30b362a298d
seeds propostas: 2 Maintained; 3 Used; 1 Worn
geração: ferramenta built-in image_gen
decisão do usuário: APROVADA PARA IMPLEMENTAÇÃO, com aparência obrigatoriamente
                    metálica ("aprovado com a observação de textura que tem que
                    estar metalica")
capturas in-engine: ../validation/industrial_corridor_ceiling_v1_hd_maintained_seed2_gameplay.png;
                    ../validation/industrial_corridor_ceiling_v1_hd_maintained_seed2_close.png;
                    ../validation/industrial_corridor_ceiling_v1_hd_used_seed3_gameplay.png;
                    ../validation/industrial_corridor_ceiling_v1_hd_used_seed3_close.png;
                    ../validation/industrial_corridor_ceiling_v1_hd_worn_seed1_gameplay.png;
                    ../validation/industrial_corridor_ceiling_v1_hd_worn_seed1_close.png;
                    ../validation/industrial_corridor_ceiling_v1_hd_comparison.png
aceitação final: "pode fazer o commit que eu faço o push"
```

## Direção proposta

- painéis modulares de aço grafite com vigas, juntas e canais estruturais;
- tampas de manutenção, ventilação discreta, parafusos e relevo estampado;
- detalhe HD em três escalas, sem transformar desgaste em ruído aleatório;
- metal exposto apenas em arestas coerentes e sujeira concentrada nas juntas;
- três condições compatíveis entre si: `Maintained`, `Used` e `Worn`;
- superfície neutra preparada para receber o modelo independente já aprovado
  `Lighting/CeilingFixture`.

## Contrato técnico proposto

```text
resolução-base: 1024 × 1024
densidade: 256 texels por metro; repetição por dimensão física
canais: Albedo RGB; Normal tangent-space; Roughness/Metalness/AO empacotados
emissão: ausente
metalness: predominante nas chapas; maior nas vigas, bordas expostas e fixadores
filtragem: trilinear com mipmaps; anisotropia quando disponível
variação: base HD compartilhada + condição/offset determinístico leve por sala
```

A observação da aprovação tornou a aparência metálica um gate automatizado: ao
menos 75% dos pixels precisam atingir metalness claramente metálico, a média
não pode cair abaixo de `0.68` e arestas/fixadores expostos devem alcançar
metalness mínimo equivalente a `240/255`. Recessos e sujeira podem reduzir a
resposta local sem transformar a superfície em plástico.

O albedo não pode conter lâmpada, lente, tubo, faixa luminosa, painel emissivo,
brilho pintado, piscina de luz ou silhueta reservada para a luminária. Luz e
geometria emissiva pertencem exclusivamente a `Lighting/CeilingFixture`.

## Variantes mostradas

- seed `2`, `Maintained`: pintura uniforme e microtextura de fabricação;
- seed `3`, `Used`: riscos leves, contato e poeira moderada nas juntas;
- seed `1`, `Worn`: abrasão e sujeira mais visíveis, preservando a estrutura.

## Escopo da eventual aprovação

A aprovação autoriza somente a direção e o contrato técnico de
`Industrial Corridor/Ceiling v1 HD`. Não autoriza Floor, Door, outros
ambientes nem qualquer alteração na luminária independente já aceita.

## Prompt final

```text
Use case: stylized-concept
Asset type: professional game-environment material approval board for "Industrial Corridor / Ceiling v1 HD"
Primary request: create one polished 16:9 landscape concept sheet for a modular industrial corridor ceiling material, designed for a retro low-poly procedural FPS but rendered with crisp modern HD PBR material quality. This sheet is for visual approval before implementation.
Scene/backdrop: dark neutral presentation background; the main upper area shows a convincing first-person corridor ceiling preview under neutral non-emissive studio illumination; the lower area presents three square material variants and a small technical map strip.
Subject: neutral modular graphite-steel ceiling panels with structural ribs, recessed seams, maintenance hatches, cable-routing channels, ventilation slots, subtle bolts, brushed metal edges and restrained believable wear. The architecture must support a separate physical ceiling lamp model, but the ceiling material itself contains no lamp.
Style/medium: high-end game material concept sheet, stylized industrial realism, sharp readable forms, HD PBR surface detail, restrained retro sci-fi language, production-ready presentation.
Composition/framing: landscape approval board. Large corridor gameplay preview across the top half. Bottom row has exactly three clearly separated square variants labeled "MAINTAINED", "USED", "WORN". At one side include a compact technical strip labeled "ALBEDO", "NORMAL", "RMA", and a clear badge reading "NO FIXTURE IN ALBEDO". Keep all labels large, clean, and legible.
Lighting/mood: neutral cool-gray inspection lighting only, enough to reveal surface relief without dramatic glow. No baked illumination. No emissive response anywhere.
Color palette: charcoal graphite, gunmetal, cool dark steel, subtle desaturated blue-gray accents, tiny restrained safety-yellow maintenance marks only where useful.
Materials/textures: believable painted steel, brushed exposed metal on edges, dark rubber seams, fine scratches, micro-roughness, shallow stamped panel relief, subtle grime concentrated along seams; avoid noisy random damage. The three variants share the same panel language and differ only by wear level.
Text (verbatim): "INDUSTRIAL CORRIDOR / CEILING v1 HD", "MAINTAINED", "USED", "WORN", "ALBEDO", "NORMAL", "RMA", "NO FIXTURE IN ALBEDO"
Constraints: show only the ceiling material family; modular and tile-friendly; the ceiling remains visually neutral where independent lamp geometry can be mounted; exactly three wear variants; no branding; no watermark; text must be rendered verbatim and not duplicated.
Avoid: lamps, light fixtures, fluorescent tubes, LED strips, glowing panels, luminous shapes, emissive pixels, baked light pools, painted-on light sources, fixture silhouettes, bright white rectangles that could be mistaken for lamps, decorative screens, windows, skylights, excessive rust, fantasy ornament, photorealistic live-action scene.
```

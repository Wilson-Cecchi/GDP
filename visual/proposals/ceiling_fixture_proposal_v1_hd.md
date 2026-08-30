# Lighting / Ceiling Fixture — proposta v1 HD

```text
unidade visual: Lighting/CeilingFixture
versão da proposta: v1 HD
arquivo/prancha: visual/proposals/ceiling_fixture_proposal_v1_hd.png
estado: ACEITA NO JOGO
data: 2026-08-30
sha256: d674a82dd2a2d72e1479a8259064e38707650d42c6a3d2e3e8e2ca4c1518c42d
captura do baseline separado: visual/validation/ceiling_fixture_separation_technical_baseline.png
capturas da implementação: visual/validation/ceiling_fixture_v1_hd_on.png;
                           visual/validation/ceiling_fixture_v1_hd_off.png;
                           visual/validation/ceiling_fixture_v1_hd_alert.png
```

## Escopo

Esta proposta cobre somente o modelo e o material da luminária de teto. Não
aprova nem modifica artisticamente qualquer teto, parede, piso ou outra fonte
de luz. A mesma base física é usada nos estados `ON`, `OFF` e `ALERT`; apenas a
cor/intensidade emissiva muda em runtime.

## Contrato técnico proposto

- modelo retangular independente, aproximadamente `1,20 × 0,24 × 0,10 m`;
- conjunto de mapas `1024×1024`: Albedo, Normal, RMA e Emission;
- carcaça de aço pintado grafite, aro metálico, junta escura e lente fosca;
- albedo neutro, sem halo, brilho, sombra projetada ou iluminação gravada;
- emissão exclusiva da lente e controlada pelo estado do ambiente;
- `ON`: branco frio; `OFF`: emissão zero; `ALERT`: vermelho controlado;
- uma única família consistente para todos os ambientes nesta versão;
- tetos permanecem unidades neutras e nunca recebem a luminária pintada.

## Prompt reproduzível

```text
Use case: stylized-concept
Asset type: production approval board for one independent game prop material
Primary request: Create proposal V1 HD for exactly one visual unit, Lighting / CeilingFixture, a separate physical ceiling-lamp model and material for the GDP industrial first-person game. This fixture must be independent from every ceiling texture.
Scene/backdrop: clean dark neutral material-review board, no decorative environment clutter
Subject: one rugged rectangular ceiling-mounted industrial luminaire, approximately 1.2 m long, 0.24 m wide and 0.10 m deep, with a graphite powder-coated metal housing, subtle brushed-steel rim, recessed frosted polycarbonate lens, narrow gasket, realistic fasteners and restrained manufacturing detail
Style/medium: polished production-grade 3D game asset concept, HD PBR realism, consistent with cold industrial steel corridor walls
Composition/framing: landscape 3:2 approval board. Show orthographic top, underside, side and end views; one large macro close-up of housing, gasket, bevel, fastener and frosted lens; a compact technical strip labeled ALBEDO, NORMAL, RMA, EMISSION; and three small in-game state previews labeled ON, OFF, ALERT. The same base material and geometry must appear in every state.
Lighting/mood: neutral studio review lighting for the model; ON uses controlled cool-white emission only from the lens, OFF has no emission, ALERT uses restrained red emission only from the same lens
Color palette: graphite #303638, dark gasket #151A1C, brushed steel #8B9091, frosted lens #DCE5E7, cool white emission #D8F3FF, alert red #D64A3A
Materials/textures: target 1024 x 1024 texture set with separate albedo, tangent-space normal, packed roughness/metalness/AO and grayscale emission mask. The albedo itself must remain neutral and contain no baked glow.
Text (verbatim): "LIGHTING / CEILING FIXTURE", "V1 HD", "ALBEDO", "NORMAL", "RMA", "EMISSION", "ON", "OFF", "ALERT"
Constraints: exactly one fixture design; clearly feasible as a separate model and independently applied material; no lamp representation painted into any ceiling surface; no ceiling texture proposal; no baked lighting, halos, cast shadows or glow in albedo; no HUD; no logos; no watermark
Avoid: fixture duplicated inside ceiling panels, flat glowing rectangle without housing, sci-fi neon strips, excessive yellow-black hazard paint, ornate decoration, crushed blacks
```

## Gate

A prancha recebeu `APROVADA PARA IMPLEMENTAÇÃO` e as capturas in-engine foram
aceitas no jogo em 2026-08-30. A aprovação não se estende a nenhum teto; cada
família `Environment/Ceiling` continua com seu próprio gate.

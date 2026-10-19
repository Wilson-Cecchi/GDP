# Worker reference reconstruction

This is an editable 3D study of [`Worker.png`](../../../../visual/Proposta_visual_separada/Worker.png) and [`Worker.md`](../../../../visual/Proposta_visual_separada/Worker.md). The reference has four composited views, but no original mesh or UV/material source. This model is an approximation for in-game visual review, not an approved replacement for the current Worker.

## Files

- `../worker_reference.blend`: eight named rigid pivots with separate armor, saw, joints, tubes, screws, and studio cameras/lights.
- `worker_reference.isemesh`: full-detail engine mesh. Its geometry includes the bevels and tubes visible in the Blender captures.
- `worker_reference_lod1.isemesh`: optional cheaper geometry to benchmark later; do not substitute it for the full-detail review automatically.
- `worker_paint_albedo.png`: generated orange painted steel albedo derived from the reference sheet with imagegen. It is **not** pixel-extracted from the concept image.
- Captures: [`front`](../../../../visual/validation/worker_reference_blender/front.png), [`left side`](../../../../visual/validation/worker_reference_blender/left_side.png), [`back`](../../../../visual/validation/worker_reference_blender/back.png), [`right side`](../../../../visual/validation/worker_reference_blender/right_side.png).

The concept sheet cannot be used as a literal albedo atlas: it contains baked light and shadow, depth occlusion, a gray background, and no UV correspondence to this independently modeled geometry. Projecting it directly would paint background and false lighting onto panels, especially when limbs animate. The generated albedo preserves the orange, steel chips, oil and scratch language without those artifacts. Its chip layout still differs from the sheet and needs visual approval in game.

## Regeneration

From the repository root:

```bash
blender -b --factory-startup --python tools/visual/build_worker_reference.py
blender -b --factory-startup --python tools/visual/build_worker_reference.py -- --export-only --low-poly-export
```

The first command writes the `.blend`, four PNGs, and full-detail `.isemesh`; the second writes the optional LOD without rerendering. The script requires `worker_paint_albedo.png` at its current path.

## Interchange format

`ISEMESH1` is little-endian: eight ASCII magic bytes, `u32 submeshCount`, then each group has `u32 partId`, `u32 materialId`, three `float32` animation pivot coordinates, `u32 vertexCount`, `u32 indexCount`, vertices (`float32 position[3], normal[3], uv[2]`), and `u32` triangle indices. Vertices are assembled in game space: Blender `X` -> game `X`, Blender `Z` -> game `Y`, Blender `-Y` -> game `Z`. Height is scaled near 1.8 m. Parts 0–7 are torso, head, left arm, right arm, left leg, right leg, backpack and saw housing. Part 8 is the rotating saw disk, with its axle as pivot; it inherits the housing attack pose. The saw replaces the right hand as seen from the character's own perspective.

Material IDs: 0 orange painted steel, 1 dark graphite/rubber, 2 exposed steel, 3 amber emissive visor, 5 safety stripes. The image provides only albedo; roughness, metal response, emissive strength and damage response require renderer materials. The renderer must preserve the separate pivots for animation.

## Visible gaps against the concept

- Armor is built from beveled rigid plates; the sheet has more irregular formed steel and more overlapping mechanical detail.
- The saw has the large side profile, teeth, drive housing and upper guard, but the sheet's guard and exposed blade have a more complex shape.
- The rear backpack, vents, tubes, hips, knees, ankles and pistons are represented, yet the reference has denser asymmetric mechanics.
- Wear is carried by a generated albedo and selected geometric chips; individual chip locations are different from the painted sheet.
- This is a static pose study. The game renderer now rotates the imported disk around its axle and loads all parts under the current rigid-part animation. Full attack, walk and damage motion still needs in-game acceptance.

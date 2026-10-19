# IndustrialRobot reconstruction candidate

Source: `tools/visual/build_robot_reference.py`, built with Blender 5.2.2.
Reference: `visual/Proposta_visual_separada/IndustrialRobot.png` and `.md`.

From the repository root:

```bash
blender -b -t 2 --factory-startup --python tools/visual/build_robot_reference.py
./build/release/gdp_enemy_model_asset_tests assets/visual/models/robot_reference/robot_reference.isemesh
```

The script writes `assets/visual/models/robot_reference.blend`, the eight-part
`robot_reference.isemesh`, and front, left, back, right orthographic renders in
`visual/validation/robot_reference_blender/`. The mesh export is repeatable:
two successive `--export-only` runs produced the same SHA-256.

`robot_paint_albedo.png` is a separate, imagegen-assisted game material swatch
based on the reference's orange/graphite wear. It was not baked into the Blender
renders and is loaded by the optional in-game reference-model preview.

Current mesh: 30 material/part submeshes, 44,056 triangles, 53,536 vertices,
2,242,676 bytes. Game-space assembled bounds are X -0.910..0.859 m,
Y 0.000..2.454 m, Z -0.564..0.699 m. Part 7 is the four-barrel cannon;
part 2 is the claw; part 6 contains both rear modules. Weapon and backpack
extents are visual dimensions, not suggested movement collider dimensions.

This is a modeled interpretation of the concept sheet. The original 3D model
and its textures were unavailable. Angular forms, mechanical density, surface
damage, and materials remain visibly simpler than the concept image. Blender's
procedural paint noise is not baked into the runtime mesh; the optional paint
albedo and renderer's semantic material palette provide runtime color.
Orthographic renders and a parser
check do not constitute in-game visual acceptance. Gameplay hitboxes were not
changed.

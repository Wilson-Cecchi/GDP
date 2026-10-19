# Material albedo candidates

These imagegen-assisted images are **review candidates**, derived from the
user's four-view concept sheets. They are material swatches, not UV projections
or exact copies of the artwork. Source Blender meshes remain editable.

| Role | Runtime file | Source sheet | Mode | Prompt summary |
| --- | --- | --- | --- | --- |
| Guard | `guard_reference/guard_paint_albedo.png` | `visual/Proposta_visual_separada/Guard.png` | precise-object-edit | Seamless pale sand/tan worn painted armor; irregular chips exposing charcoal steel, scratches, oil and grime; flat diffuse material only. |
| Engineer | `engineer_reference/engineer_paint_albedo.png` | `visual/Proposta_visual_separada/Engineer.png` | precise-object-edit | Seamless desaturated turquoise industrial paint, chips exposing dark steel, fine scratches, orange rust points; flat diffuse material only. |
| IndustrialRobot | `robot_reference/robot_paint_albedo.png` | `visual/Proposta_visual_separada/IndustrialRobot.png` | precise-object-edit | Seamless burnt safety-orange and dark graphite heavy wear, pits, soot, abrasion and exposed steel; flat diffuse material only. |

All prompts excluded robot silhouettes, weapons, scenery, labels, borders,
shadows, highlights and baked normal effects. The game renderer samples the
images on semantic painted-armor batches in the optional reference-model mode.

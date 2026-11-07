# Player Sniper review asset

`player_sniper.blend` is the editable Blender source. Regenerate it and the
`player_sniper.isemesh` game export from the repository root:

```bash
blender -b --factory-startup --python tools/visual/build_player_sniper.py
```

The orange armor uses the same packed worn-paint image as the Worker reference.
The model follows `visual/Proposta_visual_separada/Sniper.png` and preserves the
approved tall multi-part scope profile. This is an approximate game model, not
geometry extracted from the concept PNG.

The export has 3,656 triangles and 12 material/part groups. Game coordinates:
X right, Y up, Z forward. Rigid parts: 0 body, 1 magazine, 2 bolt, 9 grip.
Material IDs 0 through 5 match `WeaponMaterial`; integration and native
visual acceptance are separate steps.

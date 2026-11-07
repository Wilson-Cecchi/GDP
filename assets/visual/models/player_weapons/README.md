# Player weapons: Blender sources

The seven player weapons are editable Blender models, following the modular,
worn industrial language of the enemy reference models. The orange material
uses a packed copy of `models/worker_reference/worker_paint_albedo.png` in each
`.blend`; the game samples that same albedo on painted parts. The concept PNGs
are references, not texture atlases or exact mesh sources.

The `build_player_*.py` scripts recreate their `.blend` files from code and
overwrite manual Blender edits. Use them only to generate a fresh source model:

```bash
blender -b --factory-startup --python tools/visual/build_player_rifle.py
blender -b --factory-startup --python tools/visual/build_player_pistol.py
blender -b --factory-startup --python tools/visual/build_player_knife.py
blender -b --factory-startup --python tools/visual/build_player_sniper.py
blender -b --factory-startup --python tools/visual/build_player_shotgun.py
blender -b --factory-startup --python tools/visual/build_player_smg.py
blender -b --factory-startup --python tools/visual/build_player_grenade.py
```

Each script writes a `.blend` and an `ISEMESH1` game export in its own folder.
After editing a `.blend` by hand, export that saved file without regenerating
it. For example, from the repository root:

```bash
blender -b assets/visual/models/player_weapons/pistol/player_pistol.blend \
  --python tools/visual/export_player_weapon.py -- \
  --weapon pistol \
  --output assets/visual/models/player_weapons/pistol/player_pistol.isemesh
```

Use the corresponding weapon name and file paths for `rifle`, `knife`,
`sniper`, `shotgun`, `smg`, and `grenade`. The exporter reads the evaluated
meshes, object `weapon_part` (or `grenade_part`) property and material ID
prefixes; it does not save or change the `.blend` source.

The game loads the exports at startup. Part 0 is the fixed body, part 1 a
removable magazine where applicable, part 2 an animated mechanism, and part 9
the grip used for hand contact. The shotgun's shell is generated separately
during the per-round reload. The grenade uses one model for first-person and
in-flight rendering; its ring is a detachable group. If an export is absent or
invalid, the older procedural model remains available as a fallback.

Triangle counts for the current edited sources: grenade 5,780; rifle 2,908;
pistol 2,688; knife 860; sniper 3,684; shotgun 3,544; SMG 2,996.
Each uses at most six material IDs.
These are review assets; offscreen captures and automated tests do not replace
visual and performance review in a normal game session.

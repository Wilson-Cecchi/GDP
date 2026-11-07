# Player shotgun source

`player_shotgun.blend` is the editable source; `player_shotgun.isemesh` is the
engine export. Regenerate both with:

```bash
blender -b --factory-startup --python tools/visual/build_player_shotgun.py
```

The orange material packs `worker_paint_albedo.png` into the Blend file to
match the enemy family. The export uses weapon material IDs 0–5 and rigid
parts 0 (body), 2 (pump), and 9 (grip contact). Part 1 is intentionally
absent: the existing reload logic creates the visible transient shell.

Current export: 3,632 triangles, 14 part/material groups. Game integration
and native visual review remain separate from this source export.

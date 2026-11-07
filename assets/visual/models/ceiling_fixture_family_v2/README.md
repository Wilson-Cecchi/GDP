# CeilingFixture family v2 — runtime mesh exports

These nine `ISEMESH1` files are derived from the approved Blender proposal
[`visual/proposals/ceiling_fixture_family_v2/ceiling_fixture_family_v2.blend`](../../../../visual/proposals/ceiling_fixture_family_v2/ceiling_fixture_family_v2.blend).
The renderer loads these meshes for the room's theme or special role. Technical
captures confirm the draw path; in-game visual acceptance is a separate
playtest decision.

| Room selection | File | Triangles |
| --- | --- | ---: |
| Industrial | `industrial.isemesh` | 1,208 |
| Storage | `storage.isemesh` | 1,640 |
| Maintenance | `maintenance.isemesh` | 1,316 |
| Laboratory | `laboratory.isemesh` | 1,208 |
| Cafeteria | `cafeteria.isemesh` | 1,208 |
| Security | `security.isemesh` | 1,556 |
| Office | `office.isemesh` | 1,208 |
| Boss/Arena role override | `boss_arena.isemesh` | 1,640 |
| Central Hub role override | `central_hub.isemesh` | 1,316 |

The exporter reads only the **Family board** scene and selects each housing by
its `room_variant`, `material_condition=Used`, and `emission_state=ON`
properties. Each file contains only the objects sharing that housing's prefix.
The source `.blend` is read only. Condition and ON/OFF/ALERT are runtime choices;
there are no separate condition or state meshes.

## Binary contract

`ISEMESH1` is little-endian: eight-byte magic, `u32 submeshCount`, then for
each submesh `u32 partId`, `u32 materialId`, three `f32` pivot coordinates,
`u32 vertexCount`, `u32 indexCount`, eight `f32` per vertex (position XYZ,
normal XYZ, UV), and `u32` triangle indices. Modifiers are evaluated, normals
are transformed, and every face is triangulated during export.

- `partId=0`, pivot `(0,0,0)` for every group; fixture is rigid.
- `materialId=0` paint, `1` metal, `2` gasket and diffuser dust, `3`
  fasteners, `4` **separate emissive diffuser lens**. ID 5 is unused.
- Horizontal atlas ranges: paint `0.02..0.48`, metal/fasteners
  `0.52..0.66`, gasket/dust `0.69..0.74`, lens `0.77..0.98`; vertical
  coordinates remain inside `0.02..0.98`. Source models have no production
  UV unwrap, so the exporter uses a per-face planar projection inside each
  semantic strip. UV scale and seams need inspection in the game.
- Blender `(x,y,z)` becomes game `(x,z,-y)` without reflection or winding
  reversal. Coordinates are in metres, local to the **center of the housing**.
  The runtime positions the mesh at the light origin and adds `+0.095 m` to
  its vertical placement; this offset is **not** baked into these files.

Assembled X spans `1.20 m` and Z spans `0.28–0.30 m`. Most fixtures span
about `0.125–0.17 m` vertically. The Security mesh has two suspension rods
and ceiling mounts, extending from local Y `-0.1065` to `+0.35 m`.
In the Security T preview, its light origin is `0.44 m` below the ceiling so
the rods are visible. Other Security rooms retain their existing placement.
This geometry does not add collision.

## Reproduce and validate

From the repository root:

```bash
blender -b -t 2 --python-exit-code 1 \
  visual/proposals/ceiling_fixture_family_v2/ceiling_fixture_family_v2.blend \
  --python tools/blender/export_ceiling_fixture_family_v2.py
```

The exporter validates every file after writing: magic, header and payload
lengths, index and triangle counts, finite vertices and normals, atlas ranges,
the separate lens group, and bounds. Expected final line:
`EXPORTED 9 ISEMESH1 fixtures`. Two consecutive runs produced identical
SHA-256 hashes for all nine files.

`gdp_enemy_model_asset_tests` can parse the binary but its full test expects an
**animated enemy** with eight body parts and human-scale bounds. It fails for a
rigid ceiling fixture by design and is not this asset's acceptance test. The
runtime loader opened all nine files in a NVIDIA Quadro K2100M smoke run.
Native OpenGL captures for Storage, Maintenance, Laboratory, Security and Hub
were recorded at FOV 60/100 in POTATO/ULTRA under
`visual/validation/ceiling_fixture_family_v2/`. The other four files passed
export and loader checks but have no individual native capture yet. Studio
renders in the proposal are concept reviews, not in-game captures.

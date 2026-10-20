"""Independent structural reader for assets/visual/models/player_rig/*.isplayer.

Run: python3 tools/blender/validate_player_rig_export.py [path]
"""

import math
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FILE = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "assets/visual/models/player_rig/player_rig.isplayer"
data = FILE.read_bytes()
assert len(data) <= 32 * 1024 * 1024
offset = 0


def take(fmt):
    global offset
    size = struct.calcsize(fmt)
    assert offset + size <= len(data), (offset, size, len(data))
    value = struct.unpack_from(fmt, data, offset)
    offset += size
    return value


assert data[:8] == b"ISPLYR01"
offset = 8
bone_count, mesh_count, clip_count = take("<3I")
assert (bone_count, mesh_count, clip_count) == (19, 6, 6)
counts = []
faces_checked = 0
faces_negative = 0
faces_degenerate = 0
for material_id in range(mesh_count):
    mat, vertex_count, index_count = take("<3I")
    assert mat == material_id
    assert 0 < vertex_count < 500_000
    assert 0 < index_count < 1_500_000 and index_count % 3 == 0
    vertices = [take("<8f") for _ in range(vertex_count)]
    indices = take(f"<{index_count}I")
    assert max(indices) < vertex_count
    for position_normal_uv in vertices:
        assert all(math.isfinite(v) for v in position_normal_uv)
        x, y, z, nx, ny, nz, u, v = position_normal_uv
        assert -2 < x < 2 and -2 < y < 2 and -2 < z < 2
        assert .7 < nx*nx+ny*ny+nz*nz < 1.3
        joint = int(math.floor(u*.5))
        weight = u - 2*joint
        assert 0 <= joint < bone_count
        assert -.0001 <= weight <= 1.0001
        assert v == int(v) and 0 <= int(v) < bone_count
    for i in range(0, index_count, 3):
        a, b, c = (vertices[indices[i+j]] for j in range(3))
        ux, uy, uz = (b[j]-a[j] for j in range(3))
        vx, vy, vz = (c[j]-a[j] for j in range(3))
        cross = (uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx)
        area2 = math.sqrt(sum(q*q for q in cross))
        if area2 < 1e-10:
            faces_degenerate += 1
            continue
        average_normal = tuple((a[j+3]+b[j+3]+c[j+3])/3 for j in range(3))
        orientation = sum(cross[j]*average_normal[j] for j in range(3))
        if orientation < -1e-9:
            faces_negative += 1
        faces_checked += 1
    counts.append((vertex_count, index_count))

assert faces_checked > 1000
assert counts == [(1148, 3294), (124, 720), (3196, 5304),
                  (4608, 6768), (446, 924), (1956, 4656)], counts
assert faces_checked == 7222
assert faces_negative == 0, f"inverted triangles: {faces_negative}/{faces_checked}"
assert faces_degenerate == 0, f"degenerate triangles: {faces_degenerate}"

expected_clips = ("Idle", "Walk", "Jog", "Crouch", "Jump", "Draw_Primary_Back")
clip_summary = []
for expected in expected_clips:
    raw_name, = take("<32s")
    assert raw_name.endswith(b"\0")
    name = raw_name.split(b"\0", 1)[0].decode("ascii")
    assert name == expected
    frame_count, start_frame = take("<2I")
    assert 1 <= frame_count <= 300 and 1 <= start_frame <= 300
    for frame in range(frame_count):
        for bone in range(bone_count):
            matrix = take("<16f")
            assert all(math.isfinite(v) for v in matrix)
            assert all(abs(matrix[i]) < 10 for i in range(16))
            assert max(abs(matrix[i]-expected_value) for i, expected_value in
                       zip((3, 7, 11, 15), (0, 0, 0, 1))) < 1e-4
            if name == "Idle" and frame == 0:
                assert max(abs(matrix[i]-(1 if i in (0, 5, 10, 15) else 0))
                           for i in range(16)) < 1e-4, bone
    clip_summary.append((name, start_frame, frame_count))

assert offset == len(data), (offset, len(data))
print("ISPLAYER_VALIDATION_OK", "bytes", len(data), "bones", bone_count,
      "meshes", counts, "faces", faces_checked, "clips", clip_summary)

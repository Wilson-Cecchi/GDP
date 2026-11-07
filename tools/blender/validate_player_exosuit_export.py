"""Independent structural validator for the ISPLYR04 exosuit preview export.

Run: python3 tools/blender/validate_player_exosuit_export.py [path]
     python3 tools/blender/validate_player_exosuit_export.py --self-test
"""

import math
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT = ROOT / "assets/visual/models/player_exosuit/player_exosuit.isplayer"
MAGIC = b"ISPLYR04"
BONES = 19
MATERIALS = 8
PARTS = 5
CLIPS = (
    ("Idle", 1, 64), ("Walk", 1, 33), ("Jog", 1, 21),
    ("Crouch", 1, 10), ("Jump", 1, 30),
    ("Draw_Primary_Back", 1, 36),
)


def need(condition, message):
    if not condition:
        raise ValueError(message)


def validate(data):
    need(20 <= len(data) < 32*1024*1024, "file size")
    need(data[:8] == MAGIC, "magic/version")
    offset = 8

    def take(fmt):
        nonlocal offset
        size = struct.calcsize(fmt)
        need(offset + size <= len(data), f"truncated at {offset}")
        result = struct.unpack_from(fmt, data, offset)
        offset += size
        return result

    bone_count, mesh_count, clip_count = take("<3I")
    need((bone_count, mesh_count, clip_count) == (BONES, PARTS*MATERIALS, len(CLIPS)),
         "header counts")
    counts = []
    negative = 0
    degenerate = 0
    triangles = 0
    for group_id in range(mesh_count):
        mat, vertex_count, index_count = take("<3I")
        need(mat == group_id % MATERIALS, f"material at group {group_id}")
        need(0 <= vertex_count < 500_000, f"vertex count group {group_id}")
        need(0 <= index_count < 1_500_000 and index_count % 3 == 0,
             f"index count group {group_id}")
        need((vertex_count == 0) == (index_count == 0),
             f"empty group {group_id}")
        vertices = [take("<8f") for _ in range(vertex_count)]
        indices = take(f"<{index_count}I")
        need(not indices or max(indices) < vertex_count, f"index bounds group {group_id}")
        for vertex in vertices:
            need(all(math.isfinite(x) for x in vertex), f"nonfinite vertex group {group_id}")
            x, y, z, nx, ny, nz, encoded0, encoded1 = vertex
            need(all(-2 < value < 2 for value in (x, y, z)),
                 f"position group {group_id}")
            need(.7 < nx*nx+ny*ny+nz*nz < 1.3, f"normal group {group_id}")
            bone0 = int(math.floor(encoded0*.5))
            weight0 = encoded0 - 2*bone0
            need(0 <= bone0 < BONES and -.0001 <= weight0 <= 1.0001,
                 f"first influence group {group_id}")
            need(encoded1 == int(encoded1) and 0 <= int(encoded1) < BONES,
                 f"second influence group {group_id}")
        for i in range(0, index_count, 3):
            a, b, c = (vertices[indices[i+j]] for j in range(3))
            u = tuple(b[j]-a[j] for j in range(3))
            v = tuple(c[j]-a[j] for j in range(3))
            cross = (u[1]*v[2]-u[2]*v[1],
                     u[2]*v[0]-u[0]*v[2],
                     u[0]*v[1]-u[1]*v[0])
            area2 = math.sqrt(sum(value*value for value in cross))
            if area2 < 1e-10:
                degenerate += 1
                continue
            average_normal = tuple((a[j+3]+b[j+3]+c[j+3])/3 for j in range(3))
            if sum(cross[j]*average_normal[j] for j in range(3)) < -1e-9:
                negative += 1
            triangles += 1
        counts.append((vertex_count, index_count))
    need(10_000 <= triangles <= 30_000, "triangle budget")
    need(degenerate == 0, f"degenerate triangles {degenerate}")
    need(negative == 0, f"inverted triangles {negative}")
    for part in range(PARTS):
        need(sum(counts[part*MATERIALS+mat][1] for mat in range(MATERIALS)) > 0,
             f"empty part {part}")
    for mat in range(MATERIALS):
        need(counts[2*MATERIALS+mat][1] == counts[3*MATERIALS+mat][1],
             f"arm mismatch material {mat}")
    need(counts[4*MATERIALS+7][1] > 0, "helmet visor absent")
    need(counts[0*MATERIALS+6][1] > 0, "torso paint absent")
    clip_summary = []
    for expected_name, expected_start, expected_end in CLIPS:
        raw_name, = take("<32s")
        need(raw_name.endswith(b"\0"), "clip name termination")
        try:
            name = raw_name.split(b"\0", 1)[0].decode("ascii")
        except UnicodeDecodeError as error:
            raise ValueError("clip name encoding") from error
        need(name == expected_name, f"clip order/name {name}")
        frame_count, start_frame = take("<2I")
        need(start_frame == expected_start
             and frame_count == expected_end-expected_start+1,
             f"clip range {name}")
        for frame in range(frame_count):
            for bone in range(bone_count):
                matrix = take("<16f")
                need(all(math.isfinite(value) and abs(value) < 10 for value in matrix),
                     f"matrix finite/range {name}:{frame}:{bone}")
                need(max(abs(matrix[i]-expected) for i, expected in
                         zip((3, 7, 11, 15), (0, 0, 0, 1))) < 1e-4,
                     f"matrix affine {name}:{frame}:{bone}")
                if name == "Idle" and frame == 0:
                    need(max(abs(matrix[i]-(1 if i in (0, 5, 10, 15) else 0))
                             for i in range(16)) < 1e-4,
                         f"idle bind {bone}")
        clip_summary.append((name, start_frame, frame_count))
    need(offset == len(data), f"trailing bytes {len(data)-offset}")
    return {"bytes": len(data), "bones": bone_count, "mesh_counts": counts,
            "triangles": triangles, "clips": clip_summary}


def self_test(data):
    # Mutate one independent contract in memory for each failure class.
    cases = {}
    cases["magic"] = b"X" + data[1:]
    cases["truncated"] = data[:-1]
    cases["trailing"] = data + b"\0"
    nonfinite = bytearray(data)
    struct.pack_into("<f", nonfinite, 32, float("nan"))
    cases["nan_vertex"] = bytes(nonfinite)
    bad_weight = bytearray(data)
    struct.pack_into("<f", bad_weight, 32+24, 1.5)
    cases["bad_weight"] = bytes(bad_weight)
    _, first_vertex_count, _ = struct.unpack_from("<3I", data, 20)
    bad_index = bytearray(data)
    struct.pack_into("<I", bad_index, 20+12+32*first_vertex_count,
                     first_vertex_count)
    cases["bad_index"] = bytes(bad_index)
    for name, changed in cases.items():
        try:
            validate(changed)
        except ValueError:
            pass
        else:
            raise AssertionError(f"accepted corrupt {name}")
    return tuple(cases)


if __name__ == "__main__":
    args = sys.argv[1:]
    do_self_test = "--self-test" in args
    args = [arg for arg in args if arg != "--self-test"]
    need(len(args) <= 1, "usage: [path] [--self-test]")
    path = Path(args[0]) if args else DEFAULT
    data = path.read_bytes()
    report = validate(data)
    print("ISPLAYER04_VALIDATION_OK", path, report)
    if do_self_test:
        print("ISPLAYER04_CORRUPTION_TEST_OK", self_test(data))

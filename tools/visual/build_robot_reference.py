"""Rebuild an IndustrialRobot visual candidate from the four-view reference.

Run from repository root:
  blender -b -t 2 --factory-startup --python tools/visual/build_robot_reference.py

Outputs an editable .blend, four orthographic PNGs, and an ISEMESH1 runtime
mesh. Geometry and semantic materials are exported; Blender's procedural paint
wear is a render study, not a baked runtime texture. This is a reconstruction,
not the original model used for the concept image.
"""

from collections import defaultdict
from pathlib import Path
import math
import random
import struct
import sys

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
MODEL_DIR = ROOT / "assets/visual/models"
EXPORT_DIR = MODEL_DIR / "robot_reference"
RENDER_DIR = ROOT / "visual/validation/robot_reference_blender"
for directory in (MODEL_DIR, EXPORT_DIR, RENDER_DIR):
    directory.mkdir(parents=True, exist_ok=True)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
random.seed(8043)

# Blender: X right, Y depth (front = -Y), Z up.
# Game: X right, Y up, Z front. The boss reaches about 2.45 game metres.
GAME_SCALE = 2.45 / 5.99
PARTS = {
    0: ("torso", (0, 0, 3.66)),
    1: ("sensor_head", (0, -.16, 4.91)),
    2: ("claw_arm", (1.09, 0, 4.17)),
    3: ("cannon_arm", (-1.09, 0, 4.17)),
    4: ("left_leg", (.62, 0, 2.66)),
    5: ("right_leg", (-.62, 0, 2.66)),
    6: ("rear_modules", (0, .57, 4.44)),
    7: ("multi_barrel_cannon", (-1.64, -.55, 2.61)),
}


def material(name, rgb, metal=.0, rough=.7, glow=0.0, wear=False):
    result = bpy.data.materials.new(name)
    result.diffuse_color = (*rgb, 1)
    result.use_nodes = True
    nodes = result.node_tree.nodes
    links = result.node_tree.links
    shader = nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*rgb, 1)
    shader.inputs["Metallic"].default_value = metal
    shader.inputs["Roughness"].default_value = rough
    if glow:
        shader.inputs["Emission Color"].default_value = (*rgb, 1)
        shader.inputs["Emission Strength"].default_value = glow
    if wear:
        tex = nodes.new("ShaderNodeTexNoise")
        tex.inputs["Scale"].default_value = 53
        tex.inputs["Detail"].default_value = 4
        ramp = nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position = .35
        ramp.color_ramp.elements[0].color = (*rgb, 1)
        ramp.color_ramp.elements[1].position = .67
        ramp.color_ramp.elements[1].color = (.055, .052, .048, 1)
        ramp.color_ramp.elements.new(.52).color = (rgb[0]*.72, rgb[1]*.65, rgb[2]*.61, 1)
        links.new(tex.outputs["Fac"], ramp.inputs["Fac"])
        links.new(ramp.outputs["Color"], shader.inputs["Base Color"])
        bump = nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = .11
        bump.inputs["Distance"].default_value = .013
        links.new(tex.outputs["Fac"], bump.inputs["Height"])
        links.new(bump.outputs["Normal"], shader.inputs["Normal"])
    return result


# Semantic runtime IDs: orange, graphite, bare steel, glowing amber, rubber,
# desaturated hazard paint. Material 5 doubles as readable warning stripes.
MATERIALS = {
    0: material("scarred industrial orange", (.69, .215, .035), .48, .68, wear=True),
    1: material("charcoal armored structure", (.065, .059, .054), .72, .48),
    2: material("machined and abraded steel", (.32, .29, .25), .83, .44),
    3: material("amber reactor and optical glow", (1.0, .30, .012), .15, .24, 3.0),
    4: material("black articulated seals", (.024, .023, .021), .20, .84),
    5: material("faded ochre warning paint", (.39, .24, .078), .37, .78, wear=True),
}

ROOTS = {}
for pid, (name, pivot) in PARTS.items():
    empty = bpy.data.objects.new(name + " pivot", None)
    bpy.context.collection.objects.link(empty)
    empty.location = pivot
    empty.empty_display_type = "ARROWS"
    empty.empty_display_size = .17
    empty["part_id"] = pid
    ROOTS[pid] = empty


def attach(obj, pid, mid):
    obj.data.materials.append(MATERIALS[mid])
    obj["part_id"] = pid
    obj["material_id"] = mid
    obj.parent = ROOTS[pid]
    obj.matrix_parent_inverse = ROOTS[pid].matrix_world.inverted()
    return obj


def box(name, center, size, mid, pid, bevel=.025, rotation=None):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if rotation is not None:
        obj.rotation_euler = rotation
    if bevel:
        edge = obj.modifiers.new("forged chamfer", "BEVEL")
        edge.width = bevel
        edge.segments = 2
        obj.modifiers.new("weighted face normals", "WEIGHTED_NORMAL")
    return attach(obj, pid, mid)


def cylinder(name, center, radius, depth, mid, pid, axis=(0,0,1), sides=16):
    bpy.ops.mesh.primitive_cylinder_add(vertices=sides, radius=radius, depth=depth, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler = Vector((0,0,1)).rotation_difference(Vector(axis)).to_euler()
    edge = obj.modifiers.new("machined rim", "BEVEL")
    edge.width = min(.016, radius*.13)
    edge.segments = 2
    obj.modifiers.new("weighted face normals", "WEIGHTED_NORMAL")
    return attach(obj, pid, mid)


def rod(name, a, b, radius, mid, pid, sides=12):
    direction = Vector(b)-Vector(a)
    return cylinder(name, (Vector(a)+Vector(b))*.5, radius,
                    direction.length, mid, pid, direction, sides)


def polyplate(name, outline, y, depth, mid, pid, bevel=.02):
    """An XZ front/back armor panel with beveled real geometry."""
    count = len(outline)
    verts = [(x,y-depth*.5,z) for x,z in outline]
    verts += [(x,y+depth*.5,z) for x,z in outline]
    faces = [tuple(reversed(range(count))), tuple(range(count,2*count))]
    faces += [(i,(i+1)%count,(i+1)%count+count,i+count) for i in range(count)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    if bevel:
        edge = obj.modifiers.new("plate rolled edge", "BEVEL")
        edge.width = bevel
        edge.segments = 2
        obj.modifiers.new("weighted face normals", "WEIGHTED_NORMAL")
    return attach(obj, pid, mid)


def fastener(name, point, axis, pid, radius=.045):
    cylinder(name+" black seat",point,radius*1.55,.014,1,pid,axis,12)
    cylinder(name+" steel octagonal head",Vector(point)+Vector(axis)*.012,
             radius,.025,2,pid,axis,8)


# Enormous but segmented central chassis; the orange plates stand proud of the
# dark joints. The short sensor is buried between armored shoulder rails.
box("spinal chassis",(0,.10,3.70),(1.88,1.13,1.88),1,0,.10)
box("lower hydraulic abdomen",(0,-.12,3.02),(1.55,.92,.65),1,0,.055)
polyplate("massive upper chest slab",[(-.91,4.57),(-.72,4.83),(.72,4.83),(.91,4.57),(.76,4.12),(-.76,4.12)],-.53,.24,0,0,.06)
polyplate("center sacrificial chest armor",[(-.63,4.18),(.63,4.18),(.68,3.52),(.42,3.37),(-.42,3.37),(-.68,3.52)],-.69,.26,0,0,.05)
polyplate("chest left inset fracture panel",[(-.59,4.12),(-.08,4.12),(-.08,3.56),(-.49,3.58)],-.838,.022,5,0,.007)
polyplate("chest right inset fracture panel",[(.08,4.12),(.59,4.12),(.49,3.58),(.08,3.56)],-.838,.022,0,0,.007)
box("central chest vertical seam",(0,-.865,3.84),(.040,.024,.56),1,0,.003)
box("inset dark abdomen grille",(0,-.66,3.17),(1.14,.10,.40),1,0,.014)
for z in (3.03,3.13,3.23,3.33):
    box("abdomen horizontal intake",(0,-.726,z),(1.00,.025,.035),2,0,.003)
for side in (-1,1):
    x=side*.90
    box("shoulder root forged flange",(x,-.10,4.42),(.44,.87,.77),2,0,.065)
    polyplate("orange chest side cheek",[(side*.73,4.64),(side*1.05,4.58),(side*1.10,3.82),(side*.81,3.79)],-.58,.20,0,0,.028)
    box("vertical chest steel rail",(side*.81,-.72,4.14),(.080,.09,.79),2,0,.012)
    for z in (3.58,4.00,4.48):
        fastener("chest exposed bolt",(side*.68,-.83,z),(0,-1,0),0,.034)
    for z in (3.80,4.06,4.32):
        box("dark protected side vent",(side*.88,-.704,z),(.16,.026,.044),1,0,.003)
    cylinder("waist yaw axle",(side*.66,.08,2.95),.30,.29,2,0,(1,0,0),20)
    box("upper waist orange buckle",(side*.47,-.69,2.95),(.37,.19,.30),0,0,.027)
box("pelvis cross member",(0,.02,2.74),(1.58,.99,.54),1,0,.06)
polyplate("pelvis groin shield",[(-.52,2.91),(.52,2.91),(.39,2.39),(-.39,2.39)],-.53,.19,0,0,.03)

# Low armored optical unit: thin amber core inside a dark recess and substantial
# brow, rather than a humanoid face.
box("neck buried in chassis",(0,-.07,4.75),(.67,.62,.24),4,1,.02)
box("small sensor casing",(0,-.22,4.98),(.79,.67,.54),1,1,.052)
polyplate("overhanging sensor brow",[(-.47,5.23),(.47,5.23),(.43,5.10),(-.43,5.10)],-.55,.23,0,1,.02)
box("deep optical recess",(0,-.612,4.97),(.57,.07,.32),1,1,.012)
box("amber central optic",(0,-.655,4.98),(.31,.025,.18),3,1,.014)
box("dark optic pupil",(0,-.674,4.98),(.065,.015,.105),1,1,.005)
for side in (-1,1):
    polyplate("sensor cheek armor",[(side*.29,5.14),(side*.48,5.12),(side*.40,4.75),(side*.27,4.75)],-.59,.13,0,1,.015)
    cylinder("sensor side knuckle",(side*.46,-.17,4.96),.20,.20,2,1,(1,0,0),16)
    fastener("sensor face stud",(side*.34,-.689,5.16),(0,-1,0),1,.024)


def leg(side,pid):
    x=side*.63
    cylinder("huge exposed hip turntable",(x,.02,2.58),.42,.50,2,pid,(1,0,0),20)
    cylinder("hip black bearing well",(x+side*.29,.02,2.58),.28,.04,1,pid,(1,0,0),18)
    box("thigh internal girder",(x,.06,2.16),(.62,.72,.95),1,pid,.06)
    polyplate("thick angled thigh shield",[(x-.40,2.49),(x+.40,2.49),(x+.46,1.90),(x+.32,1.68),(x-.33,1.68),(x-.46,1.91)],-.43,.20,0,pid,.043)
    box("deep thigh center inset",(x,-.548,1.95),(.42,.023,.25),1,pid,.012)
    box("raised thigh knee service flange",(x,-.569,1.84),(.46,.06,.11),2,pid,.009)
    box("outer thigh steel cheek",(x+side*.45,.03,2.06),(.15,.72,.67),2,pid,.024)
    cylinder("enormous knee gear",(x,.02,1.56),.33,.75,2,pid,(1,0,0),20)
    cylinder("knee graphite central well",(x+side*.41,.02,1.56),.20,.025,1,pid,(1,0,0),20)
    box("shin internal piston cage",(x,.03,1.03),(.60,.71,.94),1,pid,.05)
    polyplate("orange laminated shin front",[(x-.34,1.41),(x+.34,1.41),(x+.41,1.05),(x+.28,.54),(x-.27,.54),(x-.41,1.04)],-.46,.22,0,pid,.039)
    box("shin deep center channel",(x,-.585,1.12),(.34,.026,.37),1,pid,.010)
    for height in (.94,1.10,1.26):
        box("shin steel cross strap",(x,-.606,height),(.38,.040,.050),2,pid,.004)
    polyplate("rear shin blast plate",[(x-.30,1.41),(x+.30,1.41),(x+.32,.60),(x-.32,.60)],.43,.19,0,pid,.035)
    cylinder("ankle side rotary mount",(x,.04,.50),.25,.67,2,pid,(1,0,0),18)
    box("heavy continuous boot",(x,-.26,.21),(.78,1.20,.42),1,pid,.055)
    polyplate("orange boot upper",[(x-.38,.41),(x+.38,.41),(x+.40,.14),(x-.40,.14)],-.83,.42,0,pid,.029)
    for offset in (-.25,0,.25):
        box("segmented forged toe",(x+offset,-.98,.11),(.22,.23,.17),2,pid,.012)
    box("broad steel heel",(x,.37,.16),(.72,.27,.27),2,pid,.017)
    rod("rear knee hydraulic piston",(x+side*.18,.42,1.36),(x+side*.22,.48,.65),.075,2,pid)
    for z in (.76,1.18,1.90,2.34):
        fastener("shin and thigh corner fastener",(x-side*.27,-.58,z),(0,-1,0),pid,.033)
    for i in range(2):
        box("weathered diagonal thigh caution",(x-.20+i*.20,-.566,2.30),
            (.052,.017,.27),1,pid,.004,(0,side*.40,0))


leg(1,4)
leg(-1,5)


def shoulder(side,pid):
    x=side*1.22
    cylinder("shoulder giant motor",(x,0,4.22),.44,.42,2,pid,(1,0,0),22)
    cylinder("shoulder black round inset",(x+side*.22,0,4.22),.29,.028,1,pid,(1,0,0),20)
    polyplate("high orange pauldron",[(x-side*.27,4.65),(x+side*.36,4.78),(x+side*.57,4.25),(x+side*.40,3.92),(x-side*.20,3.96)],-.13,.91,0,pid,.052)
    for index in range(3):
        box("shoulder faded diagonal black warning mark",(x-side*.13+index*.15*side,-.602,4.42),
            (.075,.017,.37),1,pid,.004,(0,side*.44,0))
    box("upper arm dark power block",(x+side*.22,-.01,3.70),(.58,.74,.76),1,pid,.055)
    box("angular bicep orange shell",(x+side*.33,-.38,3.70),(.59,.23,.72),0,pid,.032)
    cylinder("elbow open bearing",(x+side*.30,-.05,3.24),.30,.55,2,pid,(1,0,0),18)
    for z in (3.72,4.28):
        fastener("pauldron exposed mounting bolt",(x+side*.36,-.57,z),(0,-1,0),pid,.036)


shoulder(1,2)
shoulder(-1,3)

# A massy claw on the viewer's right, with three separately articulated fingers.
box("claw forearm steel hydraulic casing",(1.59,-.12,2.72),(.70,.75,1.00),1,2,.064)
polyplate("claw orange angled forearm shield",[(1.30,3.16),(1.85,3.21),(2.03,2.36),(1.72,2.24),(1.34,2.48)],-.53,.19,0,2,.036)
rod("claw exposed exterior ram",(1.94,.14,3.13),(2.01,.15,2.43),.095,2,2)
box("claw silver lower armor rail",(1.82,-.67,2.34),(.39,.07,.14),2,2,.009)
cylinder("claw wrist swivel",(1.68,-.22,2.17),.29,.33,2,2,(0,0,1),18)
box("claw palm dark piston bank",(1.68,-.28,1.94),(.53,.50,.38),1,2,.037)
for x in (1.42,1.68,1.94):
    rod("claw digit upper hydraulic",(x,-.42,1.86),(x,-.54,1.57),.090,2,2,10)
    cylinder("claw finger knuckle",(x,-.55,1.55),.105,.18,1,2,(1,0,0),10)
    rod("claw digit hooked tip",(x,-.55,1.55),(x,-.68,1.35),.082,2,2,10)
    rod("claw pointed inward talon",(x,-.68,1.35),(1.68+(x-1.68)*.72,-.76,1.27),.054,1,2,8)
for z in (2.48,2.81):
    box("claw visible orange piston collar",(1.94,-.41,z),(.10,.20,.13),0,2,.006)

# Cannon occupies the opposite silhouette and clearly shows four independently
# glowing bores and rings. The weapon is an independent rigid animation part.
box("cannon mounting yoke",(-1.57,-.19,3.05),(.70,.83,.75),1,3,.06)
rod("cannon upper exposed ram",(-1.72,-.15,3.22),(-1.72,-.61,2.88),.10,2,3)
box("cannon rear orange housing",(-1.66,-.46,2.65),(.91,.95,.87),0,7,.075)
box("cannon reactor black core",(-1.66,-.84,2.45),(.76,.82,.73),1,7,.062)
for z in (2.24,2.45,2.66):
    box("cannon side exposed machinery rib",(-2.12,-.62,z),(.07,.46,.07),2,7,.006)
for z in (2.14,2.52,2.90):
    box("cannon orange armor band",(-1.66,-.93,z),(.90,.25,.17),0,7,.018)
for dx,dz in ((-.20,-.20),(.20,-.20),(-.20,.20),(.20,.20)):
    x=-1.66+dx
    z=2.44+dz
    rod("individual steel gun tube",(x,-.53,z),(x,-1.59,z),.137,2,7,14)
    cylinder("steel gun barrel heat ring",(x,-1.13,z),.159,.075,1,7,(0,1,0),14)
    cylinder("barrel front graphite collar",(x,-1.59,z),.175,.17,1,7,(0,1,0),16)
    cylinder("amber hot bore inner",(x,-1.686,z),.098,.018,3,7,(0,1,0),14)
    cylinder("deep bore pupil",(x,-1.698,z),.035,.020,1,7,(0,1,0),12)
cylinder("four bore muzzle retaining ring",(-1.66,-1.61,2.44),.48,.13,1,7,(0,1,0),24)
for side in (-1,1):
    rod("cannon lower lateral support",(-1.66+side*.41,-.28,2.23),(-1.66+side*.41,-1.33,2.23),.055,2,7)
    box("cannon side orange splash plate",(-1.66+side*.47,-.82,2.57),(.11,.43,.71),0,7,.019)
    for z in (2.20,2.69):
        fastener("cannon side plate bolt",(-1.66+side*.54,-.96,z),(side,0,0),7,.030)

# The twin rear towers are taller than the sensor and remain separate in back
# silhouette. Louvres and orange emission are physical layers, not a flat decal.
for side in (-1,1):
    x=side*.64
    box("reactor tower forged spine",(x,.60,4.77),(.68,.82,2.08),1,6,.066)
    box("tower front orange service shell",(x,.13,4.72),(.59,.16,1.65),0,6,.033)
    box("tower front dark louvred cavity",(x,.031,5.03),(.40,.027,.83),1,6,.009)
    box("tower front exposed heat glow",(x,.009,5.03),(.25,.017,.72),3,6,.004)
    for z in (4.70,4.81,4.92,5.03,5.14,5.25,5.36):
        box("tower front heat shutter",(x,-.009,z),(.40,.037,.032),2,6,.003)
    box("tower rear armored outer lip",(x,1.07,4.81),(.72,.14,2.02),0,6,.035)
    box("rear tower dark grille cavity",(x,1.158,4.91),(.47,.035,1.30),1,6,.012)
    box("rear tower long inner glow",(x,1.185,4.99),(.25,.017,1.10),3,6,.005)
    for z in (4.48,4.64,4.80,4.96,5.12,5.28,5.44):
        box("tower rear steel horizontal shutter",(x,1.207,z),(.44,.036,.042),2,6,.003)
    box("tower lower orange vent housing",(x,1.20,4.05),(.55,.26,.47),0,6,.026)
    for z in (3.94,4.05,4.16):
        box("lower vent dark cross slat",(x,1.35,z),(.43,.023,.045),1,6,.002)
    box("tower top steel carry rail",(x,.62,5.84),(.62,.80,.065),2,6,.013)
    rod("tower top bent handrail A",(x-.25,.21,5.81),(x-.25,.28,5.99),.035,2,6)
    rod("tower top bent handrail B",(x+.25,.21,5.81),(x+.25,.28,5.99),.035,2,6)
    cylinder("side coolant transfer hub",(x+side*.43,.62,5.15),.18,.14,2,6,(1,0,0),16)
    rod("rear large exposed return pipe",(x+side*.38,1.11,5.58),(x+side*.50,1.20,3.98),.065,2,6)
    for z in (4.0,5.60):
        fastener("tower rear corner bolt",(x+side*.24,1.29,z),(0,1,0),6,.035)
box("central dorsal gearbox",(0,.98,4.48),(.48,.52,1.03),2,6,.047)
cylinder("dorsal center circular gearbox",(0,1.32,4.52),.24,.12,1,6,(0,1,0),18)
for side in (-1,1):
    rod("waist to tower hose",(side*.42,.70,3.19),(side*.64,.74,3.97),.10,4,6)

# True steel nicks are modeled on orange armor so the runtime mesh carries
# some abrasion even before a dedicated albedo texture is approved.
for pid,x_limits,z_limits,y in (
    (0,(-.48,.48),(3.47,4.08),-.831),
    (4,(.41,.86),(.67,1.33),-.584),
    (5,(-.86,-.41),(.67,1.33),-.584),
    (7,(-1.95,-1.37),(2.31,2.83),-1.07),
):
    for _ in range(22):
        x=random.uniform(*x_limits)
        z=random.uniform(*z_limits)
        box("raised bare steel edge abrasion",(x,y,z),
            (random.uniform(.018,.055),.007,random.uniform(.009,.046)),2,pid,0)


def convert(v):
    return (float(v.x), float(v.z), float(-v.y))


def game_position(v):
    return tuple(c*GAME_SCALE for c in convert(v))


def export_isemesh(path):
    bpy.context.view_layer.update()
    dg=bpy.context.evaluated_depsgraph_get()
    groups=defaultdict(lambda: ([],[]))
    vertex_maps={}
    triangles=0
    bounds_min=[math.inf]*3
    bounds_max=[-math.inf]*3
    for obj in sorted(bpy.data.objects,key=lambda item:item.name):
        if obj.type not in ("MESH","CURVE") or "part_id" not in obj:
            continue
        pid=int(obj["part_id"])
        mid=int(obj["material_id"])
        evaluated=obj.evaluated_get(dg)
        mesh=evaluated.to_mesh(preserve_all_data_layers=True,depsgraph=dg)
        if mesh is None:
            continue
        mesh.calc_loop_triangles()
        triangles+=len(mesh.loop_triangles)
        world=evaluated.matrix_world
        normal_matrix=world.to_3x3().inverted().transposed()
        vertices,indices=groups[(pid,mid)]
        lookup=vertex_maps.setdefault((pid,mid),{})
        for tri in mesh.loop_triangles:
            for loop_index in tri.loops:
                loop=mesh.loops[loop_index]
                vertex=mesh.vertices[loop.vertex_index]
                pos=game_position(world@vertex.co)
                for i in range(3):
                    bounds_min[i]=min(bounds_min[i],pos[i])
                    bounds_max[i]=max(bounds_max[i],pos[i])
                normal=convert((normal_matrix@loop.normal).normalized())
                # Stable box projection also lets a future tileable paint map
                # repeat across disconnected plates without individual unwraps.
                if abs(normal[1]) > max(abs(normal[0]),abs(normal[2])):
                    uv=(pos[0]*3,pos[2]*3)
                elif abs(normal[0]) > abs(normal[2]):
                    uv=(pos[2]*3,pos[1]*3)
                else:
                    uv=(pos[0]*3,pos[1]*3)
                # Evaluated bevel normals can differ by one float32 ULP across
                # Blender runs. Quantization stabilizes binary rebuilds.
                record=tuple(round(value,4) for value in
                             (*pos,*normal,float(uv[0]),float(uv[1])))
                index=lookup.get(record)
                if index is None:
                    index=len(vertices)
                    lookup[record]=index
                    vertices.append(record)
                indices.append(index)
        evaluated.to_mesh_clear()
    with open(path,"wb") as out:
        out.write(b"ISEMESH1")
        out.write(struct.pack("<I",len(groups)))
        for (pid,mid),(vertices,indices) in sorted(groups.items()):
            out.write(struct.pack("<II3fII",pid,mid,*game_position(Vector(PARTS[pid][1])),len(vertices),len(indices)))
            for vertex in vertices:
                out.write(struct.pack("<8f",*vertex))
            for index in indices:
                out.write(struct.pack("<I",index))
    print(f"ROBOT_SUBMESHES={len(groups)} TRIANGLES={triangles} VERTICES={sum(len(g[0]) for g in groups.values())} BYTES={path.stat().st_size}")
    print("ROBOT_BOUNDS_MIN="+",".join(f"{v:.3f}" for v in bounds_min))
    print("ROBOT_BOUNDS_MAX="+",".join(f"{v:.3f}" for v in bounds_max))


export_isemesh(EXPORT_DIR/"robot_reference.isemesh")
if "--export-only" in sys.argv:
    raise SystemExit(0)

# Neutral four-view stage, excluded from the runtime export.
neutral=material("matte neutral gray photo stage",(.27,.27,.26),0,.83)
bpy.ops.mesh.primitive_cube_add(size=1,location=(0,0,-.10))
stage=bpy.context.object
stage.name="review floor, not exported"
stage.dimensions=(40,40,.18)
bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
stage.data.materials.append(neutral)
world=bpy.context.scene.world
world.use_nodes=True
world.node_tree.nodes["Background"].inputs["Color"].default_value=(.34,.34,.33,1)
world.node_tree.nodes["Background"].inputs["Strength"].default_value=.72


def area(name,pos,power,size):
    data=bpy.data.lights.new(name,"AREA")
    data.energy=power
    data.shape="DISK"
    data.size=size
    obj=bpy.data.objects.new(name,data)
    bpy.context.collection.objects.link(obj)
    obj.location=pos
    obj.rotation_euler=(Vector((0,0,3))-obj.location).to_track_quat("-Z","Y").to_euler()


area("neutral key",(-5,-5,8),1250,6)
area("soft fill",(5,-2,6),670,5)
area("warm rear edge",(0,5,8),830,4)
camera_data=bpy.data.cameras.new("ortho comparison camera")
camera=bpy.data.objects.new("ortho comparison camera",camera_data)
bpy.context.collection.objects.link(camera)
bpy.context.scene.camera=camera
camera_data.type="ORTHO"
camera_data.ortho_scale=7.2
scene=bpy.context.scene
scene.render.engine="BLENDER_EEVEE"
scene.render.resolution_x=720
scene.render.resolution_y=900
scene.render.resolution_percentage=100
scene.render.image_settings.file_format="PNG"
scene.view_settings.view_transform="Standard"
scene.view_settings.look="Medium High Contrast"
for name,pos in {
    "front":(0,-15,3.05),
    "left_side":(-15,0,3.05),
    "back":(0,15,3.05),
    "right_side":(15,0,3.05),
}.items():
    camera.location=pos
    camera.rotation_euler=(Vector((0,0,3.04))-camera.location).to_track_quat("-Z","Y").to_euler()
    scene.render.filepath=str(RENDER_DIR/f"{name}.png")
    bpy.ops.render.render(write_still=True)

camera.location=(0,-15,3.05)
camera.rotation_euler=(Vector((0,0,3.04))-camera.location).to_track_quat("-Z","Y").to_euler()
bpy.ops.wm.save_as_mainfile(filepath=str(MODEL_DIR/"robot_reference.blend"))
print("ROBOT_RIG_PARTS="+",".join(PARTS[i][0] for i in range(8)))

"""Build a segmented Officer visual study from its four-view concept.

Run from repository root:
  blender -b --factory-startup --python tools/visual/build_officer_reference.py

Exports an editable .blend, four orthographic PNGs, and an ISEMESH1 mesh.
Blender's procedural paint wear is a render aid; ISEMESH1 records semantic
material IDs and actual geometry, including exposed edge chips.
"""

from collections import defaultdict
from pathlib import Path
import math
import random
import struct
import sys
import zlib

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
MODEL_DIR = ROOT / "assets/visual/models"
MESH_DIR = MODEL_DIR / "officer_reference"
ALBEDO_PATH = MESH_DIR / "officer_paint_albedo.png"
RENDER_DIR = ROOT / "visual/validation/officer_reference_blender"
MODEL_DIR.mkdir(parents=True, exist_ok=True)
MESH_DIR.mkdir(parents=True, exist_ok=True)
RENDER_DIR.mkdir(parents=True, exist_ok=True)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
random.seed(7041)


def build_paint_atlas(path):
    """Deterministic diffuse paint atlas: burgundy left, navy graphite right.

    This encodes color and scattered paint wear only, with no lighting, AO,
    highlights, background, or studio shadows.
    """
    rng = random.Random(7041)
    size = 512
    pixels = bytearray(size * size * 4)
    palette = ((127, 57, 50), (59, 65, 76))
    for y in range(size):
        for x in range(size):
            half = x // 256
            base = palette[half]
            grain = rng.randint(-8, 8)
            macro = int(5 * math.sin(2*math.pi*(x%256)/128)
                        * math.sin(2*math.pi*y/128))
            k = (y*size+x)*4
            for c in range(3):
                pixels[k+c] = max(0, min(255, base[c] + grain + macro))
            pixels[k+3] = 255
    # Short irregular exposed-metal nicks, more frequent along painted armor.
    for half in range(2):
        for _ in range(1050):
            x0 = rng.randrange(256)
            y0 = rng.randrange(size)
            width = rng.choice((1,1,2,2,3,4,6))
            height = rng.choice((1,1,1,2,3))
            chip = rng.choice(((81,77,71),(61,59,57),(105,92,78),(58,42,38)))
            for dy in range(height):
                for dx in range(width):
                    x = half*256 + ((x0+dx)%256)
                    y = (y0+dy)%size
                    k = (y*size+x)*4
                    pixels[k:k+4] = bytes((*chip,255))
    raw = b"".join(b"\0"+pixels[y*size*4:(y+1)*size*4] for y in range(size))
    def chunk(tag,data):
        return struct.pack(">I",len(data))+tag+data+struct.pack(">I",zlib.crc32(tag+data)&0xFFFFFFFF)
    path.write_bytes(b"\x89PNG\r\n\x1a\n"
        +chunk(b"IHDR",struct.pack(">IIBBBBB",size,size,8,6,0,0,0))
        +chunk(b"IDAT",zlib.compress(raw,9))+chunk(b"IEND",b""))


if not ALBEDO_PATH.exists():
    build_paint_atlas(ALBEDO_PATH)

# Blender X right, Y back, Z up. Runtime X right, Y up, Z front.
PARTS = {
    0: ("torso", (0, 0, 3.45)),
    1: ("head", (0, -.03, 4.47)),
    2: ("left_arm", (.96, 0, 4.06)),
    3: ("right_arm", (-.96, 0, 4.06)),
    4: ("left_leg", (.47, 0, 2.58)),
    5: ("right_leg", (-.47, 0, 2.58)),
    6: ("back", (0, .53, 3.88)),
    7: ("compact_gun", (-1.13, -.36, 1.94)),
}


def mat(name, color, metallic=0.0, roughness=.7, emit=0.0, wear=False, atlas_half=None):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    n = m.node_tree.nodes
    l = m.node_tree.links
    bsdf = n.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if emit:
        bsdf.inputs["Emission Color"].default_value = (*color, 1)
        bsdf.inputs["Emission Strength"].default_value = emit
    if atlas_half is not None:
        uv = n.new("ShaderNodeTexCoord")
        mapping = n.new("ShaderNodeMapping")
        mapping.inputs["Location"].default_value = (.002 + atlas_half*.5,0,0)
        mapping.inputs["Scale"].default_value = (.496,1,1)
        texture = n.new("ShaderNodeTexImage")
        texture.image = bpy.data.images.load(str(ALBEDO_PATH),check_existing=True)
        texture.extension = "REPEAT"
        l.new(uv.outputs["UV"],mapping.inputs["Vector"])
        l.new(mapping.outputs["Vector"],texture.inputs["Vector"])
        l.new(texture.outputs["Color"],bsdf.inputs["Base Color"])
    elif wear:
        noise = n.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 82
        noise.inputs["Detail"].default_value = 4
        ramp = n.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position = .38
        ramp.color_ramp.elements[0].color = (*color, 1)
        ramp.color_ramp.elements[1].position = .77
        ramp.color_ramp.elements[1].color = (.12, .115, .11, 1)
        ramp.color_ramp.elements.new(.60).color = (color[0]*.68, color[1]*.66, color[2]*.65, 1)
        l.new(noise.outputs["Fac"], ramp.inputs["Fac"])
        l.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
        bump = n.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = .07
        bump.inputs["Distance"].default_value = .01
        l.new(noise.outputs["Fac"], bump.inputs["Height"])
        l.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return m


# Semantic runtime IDs: burgundy, navy graphite, bare steel, amber, rubber,
# and darker secondary metal. The latter is used on the communications pack.
materials = {
    0: mat("abraded burgundy armor", (.23, .053, .043), .47, .76, atlas_half=0),
    1: mat("weathered navy graphite armor", (.043, .051, .067), .54, .72, atlas_half=1),
    2: mat("exposed brushed steel", (.125, .121, .112), .72, .48),
    3: mat("amber command electronics", (.88, .26, .004), .1, .23, emit=1.8),
    4: mat("dark joint bellows", (.023, .027, .031), .1, .92),
    5: mat("communications module charcoal", (.061, .068, .078), .55, .66, wear=True),
}

roots = {}
for part_id, (name, pivot) in PARTS.items():
    obj = bpy.data.objects.new(name + " pivot", None)
    bpy.context.collection.objects.link(obj)
    obj.location = pivot
    obj.empty_display_type = "ARROWS"
    obj.empty_display_size = .15
    obj["part_id"] = part_id
    roots[part_id] = obj


def attach(obj, pid, mid):
    obj.data.materials.append(materials[mid])
    obj["part_id"] = pid
    obj["material_id"] = mid
    obj.parent = roots[pid]
    obj.matrix_parent_inverse = roots[pid].matrix_world.inverted()
    return obj


def box(name, loc, dims, mid, pid, bevel=.014, rotation=None):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dims
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if rotation is not None:
        obj.rotation_euler = rotation
    if bevel:
        mod = obj.modifiers.new("machined edge", "BEVEL")
        mod.width = bevel
        mod.segments = 1
        mod.affect = "EDGES"
        obj.modifiers.new("weighted face normals", "WEIGHTED_NORMAL")
    return attach(obj, pid, mid)


def cyl(name, loc, radius, depth, mid, pid, axis=(0, 0, 1), sides=14):
    bpy.ops.mesh.primitive_cylinder_add(vertices=sides, radius=radius, depth=depth, location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler = Vector((0, 0, 1)).rotation_difference(Vector(axis)).to_euler()
    mod = obj.modifiers.new("machined rim", "BEVEL")
    mod.width = min(.012, radius*.12)
    mod.segments = 1
    obj.modifiers.new("weighted face normals", "WEIGHTED_NORMAL")
    return attach(obj, pid, mid)


def pipe(name, points, radius, mid, pid):
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.resolution_u = 8
    curve.bevel_depth = radius
    curve.bevel_resolution = 1
    spline = curve.splines.new("POLY")
    spline.points.add(len(points)-1)
    for point, xyz in zip(spline.points, points):
        point.co = (*xyz, 1)
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    return attach(obj, pid, mid)


def rod(name, a, b, radius, mid, pid, sides=12):
    direction = Vector(b)-Vector(a)
    return cyl(name, (Vector(a)+Vector(b))*.5, radius, direction.length, mid, pid, direction, sides)


def plate_xz(name, outline, y, depth, mid, pid, bevel=.015):
    n = len(outline)
    vertices = [(x, y-depth/2, z) for x, z in outline]
    vertices += [(x, y+depth/2, z) for x, z in outline]
    faces = [tuple(reversed(range(n))), tuple(range(n, 2*n))]
    faces += [(i, (i+1)%n, (i+1)%n+n, i+n) for i in range(n)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    uv = mesh.uv_layers.new(name="paint mapping")
    xmin,xmax=min(x for x,_ in outline),max(x for x,_ in outline)
    zmin,zmax=min(z for _,z in outline),max(z for _,z in outline)
    for poly in mesh.polygons:
        for li in poly.loop_indices:
            vert = mesh.vertices[mesh.loops[li].vertex_index].co
            uv.data[li].uv=((vert.x-xmin)/max(.001,xmax-xmin),(vert.z-zmin)/max(.001,zmax-zmin))
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    if bevel:
        mod = obj.modifiers.new("plate rim", "BEVEL")
        mod.width = bevel
        mod.segments = 1
        mod.affect = "EDGES"
        obj.modifiers.new("weighted normals", "WEIGHTED_NORMAL")
    return attach(obj, pid, mid)


def plate_yz(name, outline, x, depth, mid, pid, bevel=.015):
    """A side-facing armor plate in the depth/height plane."""
    n = len(outline)
    vertices = [(x-depth/2, y, z) for y,z in outline]
    vertices += [(x+depth/2, y, z) for y,z in outline]
    faces = [tuple(reversed(range(n))), tuple(range(n,2*n))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    uv=mesh.uv_layers.new(name="paint mapping")
    ymin,ymax=min(y for y,_ in outline),max(y for y,_ in outline)
    zmin,zmax=min(z for _,z in outline),max(z for _,z in outline)
    for poly in mesh.polygons:
        for li in poly.loop_indices:
            vert=mesh.vertices[mesh.loops[li].vertex_index].co
            uv.data[li].uv=((vert.y-ymin)/max(.001,ymax-ymin),(vert.z-zmin)/max(.001,zmax-zmin))
    obj = bpy.data.objects.new(name,mesh)
    bpy.context.collection.objects.link(obj)
    if bevel:
        mod=obj.modifiers.new("plate edge", "BEVEL")
        mod.width=bevel
        mod.segments=1
        mod.affect="EDGES"
        obj.modifiers.new("weighted normals", "WEIGHTED_NORMAL")
    return attach(obj,pid,mid)


def bolt(name, pos, axis, pid, radius=.024):
    cyl(name+" recessed washer", pos, radius*1.5, .011, 1, pid, axis, 10)
    cyl(name+" octagonal head", Vector(pos)+Vector(axis)*.009, radius, .016, 2, pid, axis, 8)


# Main torso with separate breastplate, inset vertical status lights, armature,
# collar pipes, and lower tactical storage. References put the lights at left.
box("graphite torso chassis", (0, .03, 3.63), (1.54, .77, 1.35), 1, 0, .08)
box("articulated lower spine", (0, .02, 2.94), (1.12, .52, .35), 2, 0, .03)
plate_xz("broad navy breastplate", [(-.64,4.12),(-.45,4.30),(.48,4.30),(.68,4.12),(.54,3.43),(-.57,3.43)], -.43, .12, 1, 0, .025)
plate_xz("large central command armor", [(-.34,3.90),(.39,3.91),(.49,3.61),(.29,3.29),(-.29,3.29),(-.48,3.57)], -.53, .10, 1, 0)
box("chest vertical light housing", (-.47,-.60,3.87), (.22,.15,.53), 2, 0, .008)
box("recess between status lamps", (-.47,-.683,3.87), (.16,.025,.44), 1, 0, .003)
for z in (3.71,3.84,3.97,4.10):
    box("front command indicator", (-.47,-.704,z), (.105,.011,.075), 3, 0, .006)
for side in (-1,1):
    x=side*.66
    box("chest side service rail", (x,-.48,3.65), (.12,.20,.82), 2, 0, .016)
    cyl("waist spindle", (side*.62,.02,2.88), .19,.24,2,0,(1,0,0),18)
    for z in (3.45,3.98,4.23):
        bolt("torso armored fastener", (side*.33,-.601,z),(0,-1,0),0)
box("abdominal reinforcement", (0,-.27,3.12), (1.07,.35,.30), 1,0,.02)
box("belt graphite base", (0,-.07,2.75), (1.34,.63,.29), 1,0,.03)
plate_xz("angular pelvic guard", [(-.37,2.92),(.37,2.92),(.28,2.45),(-.28,2.45)], -.40,.11,1,0)
box("burgundy belt emblem", (0,-.49,2.86), (.41,.12,.17),0,0,.015)
for x in (-.68,-.49,.48,.68):
    box("belt comms pouch", (x,-.36,2.91),(.19,.27,.29),1,0,.013)
    box("pouch clasp",(x,-.52,2.99),(.10,.035,.06),2,0,.003)
for x in (-.56,.56):
    pipe("collar command hose",[(x,.24,4.19),(x,.07,4.40),(x*.75,-.24,4.35)],.039,2,0)

# The reference has a recessed central breastplate surrounded by many short,
# individually serviceable armour segments, rather than one uninterrupted box.
plate_xz("lower left chest flank",[(-.65,3.72),(-.49,3.74),(-.42,3.34),(-.62,3.32)],-.60,.075,0,0,.014)
plate_xz("lower right chest flank",[(.46,3.75),(.64,3.71),(.62,3.31),(.42,3.34)],-.60,.075,1,0,.014)
for side in (-1,1):
    x=side*.30
    box("breastplate raised edge",(x,-.602,4.15),(.36,.045,.048),2,0,.006)
    for z in (3.50,3.63,3.76):
        box("ribbed abdominal casting",(side*.27,-.525,z),(.31,.085,.056),1,0,.009)
    pipe("front chest hydraulic hose",[(side*.57,-.43,4.17),(side*.69,-.51,3.84),(side*.56,-.43,3.39)],.024,2,0)
    bolt("lower breastplate screw",(side*.42,-.616,3.38),(0,-1,0),0,.020)
for z in (3.51,3.62,3.73,3.84,3.95):
    box("command bus vertical relief",(.50,-.642,z),(.12,.036,.045),2,0,.004)
for x in (-.55,-.37,-.18,.18,.37,.55):
    box("waist exposed actuator",(x,-.25,3.00),(.075,.18,.20),2,0,.009)

# Compact helmet: burgundy crown, armored cheeks, tiny amber optical slit.
box("neck rotary column", (0,.01,4.38),(.41,.40,.30),2,1,.025)
box("helmet graphite casting",(0,-.03,4.78),(.76,.67,.75),1,1,.06)
plate_xz("burgundy faceted crown",[(-.33,5.12),(-.23,5.29),(.24,5.29),(.34,5.13),(.32,5.00),(-.33,5.00)],-.10,.64,0,1,.025)
box("angular optic hood",(0,-.40,4.94),(.68,.30,.24),1,1,.025)
box("dark visor socket",(0,-.578,4.81),(.57,.06,.24),2,1,.012)
box("small amber visor",(0,-.616,4.83),(.42,.017,.115),3,1,.008)
box("visor central dark pupil",(0,-.632,4.83),(.055,.012,.087),1,1,.004)
box("lower muzzle and respirator",(0,-.43,4.57),(.47,.24,.19),1,1,.015)
for side in (-1,1):
    plate_xz("helmet side cheek",[(side*.26,4.80),(side*.43,4.84),(side*.37,4.54),(side*.20,4.52)],-.43,.13,1,1,.012)
    cyl("headset rotating ear hub",(side*.44,-.01,4.83),.21,.15,2,1,(1,0,0),18)
    cyl("headset dark ear cap",(side*.54,-.01,4.83),.13,.026,1,1,(1,0,0),18)
    bolt("helmet crown screw",(side*.23,-.451,5.16),(0,-1,0),1,.021)
for x in (-.11,0,.11):
    box("chin vent cut",(x,-.559,4.58),(.024,.012,.067),2,1,.002)
for side in (-1,1):
    x=side*.34
    box("visor protective side mount",(x,-.575,4.83),(.13,.11,.25),1,1,.015)
    pipe("helmet upper cable",[(side*.38,.17,5.06),(side*.45,.03,5.23),(side*.38,-.23,5.17)],.023,2,1)
    for z in (4.64,4.69,4.74):
        box("cheek intake slot",(side*.325,-.523,z),(.095,.018,.020),2,1,.002)
box("thin dark visor aperture",(0,-.629,4.83),(.38,.014,.080),1,1,.004)
box("amber optical core",(0,-.640,4.83),(.145,.014,.085),3,1,.006)


def leg(side,pid):
    x=side*.48
    cyl("hip mechanical bearing",(x,.01,2.57),.28,.34,2,pid,(1,0,0),20)
    box("thigh black framework",(x,0,2.05),(.48,.50,.82),4,pid,.035)
    plate_xz("thigh navy front plate",[(x-.24,2.42),(x+.24,2.42),(x+.29,1.88),(x+.21,1.66),(x-.22,1.66),(x-.28,1.91)],-.34,.13,1,pid,.025)
    box("thigh external side rail",(x+side*.29,.01,2.08),(.11,.53,.60),2,pid,.013)
    plate_yz("side thigh angular command armor",[(-.29,2.36),(.24,2.38),(.34,2.08),(.24,1.79),(-.26,1.80)],x+side*.37,.11,1,pid,.02)
    cyl("knee exposed hinge",(x,.02,1.56),.24,.58,2,pid,(1,0,0),20)
    cyl("knee dark outer cavity",(x+side*.31,.02,1.56),.15,.025,4,pid,(1,0,0),20)
    plate_xz("burgundy knee cap",[(x-.23,1.69),(x+.22,1.69),(x+.23,1.33),(x-.22,1.33)],-.39,.12,0,pid,.024)
    box("calf hydraulic housing",(x,.03,1.04),(.42,.50,.95),4,pid,.035)
    plate_xz("shin navy outer shell",[(x-.24,1.35),(x+.25,1.35),(x+.27,.89),(x+.19,.55),(x-.20,.55),(x-.28,.86)],-.31,.13,1,pid,.026)
    box("shin outer burgundy accent",(x+side*.26,-.12,1.09),(.09,.28,.38),0,pid,.011)
    plate_yz("side calf burgundy plate",[(-.29,1.28),(.20,1.32),(.32,.90),(.18,.61),(-.28,.62)],x+side*.33,.11,0,pid,.02)
    plate_yz("side calf navy inset",[(-.23,1.13),(.13,1.13),(.20,.83),(-.22,.82)],x+side*.402,.025,1,pid,.009)
    cyl("ankle rotary hinge",(x,.01,.48),.18,.45,2,pid,(1,0,0),16)
    box("boot internal frame",(x,-.25,.22),(.59,.94,.38),4,pid,.04)
    plate_xz("boot navy top plate",[(x-.30,.35),(x+.30,.35),(x+.34,.14),(x-.34,.14)],-.65,.34,1,pid,.02)
    for toe in (-.20,0,.20):
        box("segmented steel boot toe",(x+toe,-.77,.10),(.16,.23,.17),2,pid,.01)
    for z in (.70,1.13,1.86,2.30):
        bolt("leg plating bolt",(x-side*.17,-.414,z),(0,-1,0),pid,.023)
    for y,z in ((-.17,1.18),(.19,.77),(.16,2.22),(-.18,1.87)):
        bolt("side leg rivet",(x+side*.445,y,z),(side,0,0),pid,.021)
    rod("rear calf piston",(x+side*.13,.29,1.35),(x+side*.14,.34,.66),.052,2,pid)
    pipe("rear leg cable",[(x-side*.13,.31,2.29),(x-side*.15,.42,1.86),(x-side*.17,.35,1.43)],.029,4,pid)
    for z in (2.22,1.98,1.75):
        box("side tactical cartridge",(x+side*.42,.01,z),(.17,.31,.18),1,pid,.013)
    # Narrow seams, exposed screw rows and inset red armour over the layered
    # navy thigh/shin create the dense panel construction of the four-view art.
    plate_xz("thigh burgundy outer inset",[(x+side*.035,2.33),(x+side*.23,2.34),(x+side*.25,1.91),(x+side*.075,1.90)],-.418,.047,0,pid,.010)
    plate_xz("thigh central dark inset",[(x-side*.22,2.32),(x+side*.015,2.33),(x+side*.025,1.78),(x-side*.20,1.78)],-.417,.033,1,pid,.008)
    for z in (1.80,1.93,2.06,2.19):
        box("thigh machined transverse seam",(x-side*.10,-.448,z),(.22,.018,.016),2,pid,.002)
    cyl("outer knee concentric rotor",(x+side*.335,.02,1.55),.185,.055,2,pid,(1,0,0),22)
    cyl("knee axle boss",(x+side*.373,.02,1.55),.092,.063,1,pid,(1,0,0),18)
    for y,z in ((-.26,1.42),(-.24,1.67),(.18,1.50)):
        bolt("knee service bolt",(x+side*.391,y,z),(side,0,0),pid,.017)
    plate_xz("shin scarlet edge cap",[(x+side*.10,1.26),(x+side*.25,1.25),(x+side*.26,.82),(x+side*.13,.78)],-.405,.046,0,pid,.009)
    for z in (1.23,1.10,.97,.84,.71):
        box("shin spaced horizontal seam",(x-side*.09,-.397,z),(.22,.016,.013),2,pid,.002)
    for y,z in ((-.28,1.30),(-.25,.73)):
        bolt("shin outer shell fixing",(x+side*.394,y,z),(side,0,0),pid,.016)
    cyl("outer ankle bearing ring",(x+side*.27,.01,.48),.17,.05,2,pid,(1,0,0),16)
    box("boot top armored tongue",(x,-.63,.29),(.30,.22,.06),0,pid,.009)


leg(1,4)
leg(-1,5)


def arm(side,pid):
    x=side*1.00
    cyl("shoulder steel bearing",(x,0,4.08),.29,.36,2,pid,(1,0,0),20)
    box("shoulder flex seal",(x+side*.10,-.01,3.99),(.37,.55,.59),4,pid,.03)
    plate_xz("burgundy shoulder shield",[(x-side*.20,4.36),(x+side*.20,4.43),(x+side*.42,4.20),(x+side*.33,3.83),(x-side*.11,3.78)],-.04,.78,0,pid,.03)
    plate_yz("raised burgundy shoulder side",[(-.38,4.33),(.19,4.38),(.42,4.16),(.31,3.87),(-.34,3.89)],x+side*.43,.11,0,pid,.026)
    plate_yz("shoulder inset steel edge",[(-.30,4.25),(.14,4.27),(.25,4.17),(-.29,4.16)],x+side*.497,.025,2,pid,.008)
    box("navy shoulder bottom lip",(x+side*.20,-.44,3.84),(.42,.18,.14),1,pid,.015)
    box("upper arm internal steel rails",(x+side*.10,0,3.56),(.31,.42,.68),2,pid,.022)
    plate_xz("navy bicep plate",[(x-.17,3.86),(x+.19,3.86),(x+.16,3.33),(x-.16,3.33)],-.27,.13,1,pid,.018)
    cyl("elbow dark open joint",(x+side*.08,-.01,3.15),.20,.43,4,pid,(1,0,0),18)
    cyl("elbow steel outer ring",(x+side*.31,-.01,3.15),.15,.032,2,pid,(1,0,0),18)
    fx=x+side*.01
    box("forearm actuator",(fx,-.03,2.80),(.33,.43,.57),4,pid,.025)
    plate_xz("navy forearm vambrace",[(fx-.18,3.05),(fx+.18,3.05),(fx+.19,2.53),(fx-.19,2.53)],-.28,.14,1,pid,.02)
    box("long burgundy forearm insert",(fx+side*.065,-.37,2.81),(.20,.10,.40),0,pid,.015)
    plate_yz("side forearm burgundy plate",[(-.31,3.02),(.20,3.02),(.23,2.58),(-.29,2.58)],fx+side*.24,.09,0,pid,.016)
    plate_yz("side forearm graphite end",[(-.25,2.70),(.20,2.70),(.20,2.57),(-.26,2.57)],fx+side*.29,.022,1,pid,.006)
    cyl("wrist steel cuff",(fx,-.04,2.48),.15,.20,2,pid,(0,0,1),14)
    box("open articulated hand",(fx,-.13,2.30),(.30,.33,.29),4,pid,.025)
    for finger in (-.11,-.035,.04,.115):
        box("separated metal finger",(fx+finger,-.20,2.15),(.055,.15,.16),2,pid,.008)
    for z in (4.19,3.91,2.87):
        bolt("arm pad screw",(x+side*.045,-.465,z),(0,-1,0),pid,.025)
    for y,z in ((-.24,4.24),(.20,4.02),(-.16,2.90),(.15,2.68)):
        bolt("arm side armored fastener",(x+side*.505,y,z),(side,0,0),pid,.024)
    pipe("arm protected outer cable",[(x+side*.23,.27,4.18),(x+side*.29,.34,3.62),(x+side*.20,.25,3.17)],.03,4,pid)
    plate_xz("shoulder inset graphite slab",[(x-side*.11,4.34),(x+side*.16,4.39),(x+side*.34,4.16),(x+side*.24,3.91),(x-side*.09,3.92)],-.445,.074,1,pid,.015)
    plate_xz("shoulder scarlet upper rim",[(x-side*.17,4.40),(x+side*.18,4.44),(x+side*.32,4.27),(x-side*.13,4.29)],-.498,.045,0,pid,.010)
    for xx,zz in ((x-side*.08,4.28),(x+side*.23,4.18),(x+side*.10,3.99)):
        bolt("shoulder inset anchor",(xx,-.495,zz),(0,-1,0),pid,.017)
    for z in (3.66,3.58,3.50,3.42):
        box("bicep narrow layered armor",(x+side*.06,-.363,z),(.28,.035,.060),1,pid,.007)
    cyl("inner elbow exposed rotor",(x+side*.07,-.01,3.15),.15,.48,2,pid,(1,0,0),20)
    for y in (-.20,-.07,.07,.20):
        box("forearm machined rib",(fx,-.365+y*.04,2.92+y),(.25,.033,.027),2,pid,.004)
    plate_xz("forearm small outer shell",[(fx-side*.13,2.99),(fx+side*.17,2.99),(fx+side*.18,2.68),(fx-side*.12,2.66)],-.419,.058,0,pid,.010)
    for z in (2.74,2.91):
        bolt("forearm shell fixing",(fx-side*.08,-.459,z),(0,-1,0),pid,.016)
    pipe("forearm recessed actuator cable",[(fx+side*.19,.14,3.04),(fx+side*.21,.24,2.84),(fx+side*.17,.12,2.58)],.020,2,pid)


arm(1,2)
arm(-1,3)

# Tall command backpack and antennas. Position makes rear lights and door
# recognizable from behind rather than borrowing the front optical visor.
box("comms backpack load frame",(0,.55,3.75),(1.36,.37,1.60),1,6,.045)
box("central radio armour door",(0,.81,3.78),(.78,.18,1.26),5,6,.021)
box("burgundy top service cover",(0,.90,4.43),(.77,.12,.23),0,6,.014)
for side in (-1,1):
    x=side*.56
    box("radio side chassis",(x,.83,3.90),(.31,.35,1.39),5,6,.021)
    box("radio side steel edge",(x+side*.16,.85,3.90),(.055,.37,1.36),2,6,.006)
    for z in (3.48,3.84,4.20):
        bolt("radio cover screw",(side*.29,.923,z),(0,1,0),6,.023)
    pipe("comms pack flexible lead",[(side*.52,.76,3.20),(side*.63,.66,3.05),(side*.56,.42,2.93)],.035,1,6)
box("rear door angular crossbar",(0,.92,3.32),(.49,.06,.08),2,6,.005)
for side in (-1,1):
    x=side*.27
    rod("rear module X brace",(x-.17,.931,3.27),(x+.17,.931,3.62),.022,2,6)
    rod("rear module opposing brace",(x-.17,.934,3.62),(x+.17,.934,3.27),.022,2,6)
    for z in (3.56,3.68,3.80,3.92,4.04,4.16):
        box("radio lateral cooling fin",(side*.57,1.034,z),(.23,.035,.048),2,6,.004)
    pipe("radio external armored data lead",[(side*.62,.49,4.52),(side*.80,.78,4.43),(side*.77,1.02,3.87),(side*.69,.83,3.27)],.035,1,6)
    for z in (3.42,3.95,4.35):
        bolt("communications case corner bolt",(side*.35,1.014,z),(0,1,0),6,.023)
for x in (-.19,-.09,.01,.11,.21):
    box("rear service ventilation slat",(x,.924,4.20),(.035,.035,.15),1,6,.002)
box("back vertical status housing",(-.50,1.015,3.94),(.18,.10,.47),2,6,.009)
for z in (3.81,3.94,4.07):
    box("rear amber network lamp",(-.50,1.073,z),(.095,.014,.071),3,6,.004)
for x,height in ((-.68,5.12),(.18,5.54),(.71,5.27)):
    cyl("antenna base socket",(x,.64,4.52),.075,.17,2,6,(0,0,1),12)
    rod("telescopic command antenna",(x,.64,4.57),(x,.64,height),.026,1,6)
    cyl("antenna insulated tip",(x,.64,height),.037,.07,2,6,(0,0,1),10)
    for z in (4.72,4.84):
        cyl("antenna telescopic segment",(x,.64,z),.042,.040,2,6,(0,0,1),10)
for side in (-1,1):
    pipe("over-shoulder power conduit",[(side*.32,.88,4.42),(side*.38,.43,4.62),(side*.49,.12,4.48)],.038,2,6)

# Compact gun hangs from the right hand; a short barrel, heavy breech, and
# inset lamp separate it from the Guard's long rifle silhouette.
box("steel short weapon receiver",(-1.10,-.34,2.02),(.29,.30,.51),2,7,.018)
box("graphite weapon outer shroud",(-1.10,-.53,1.91),(.34,.16,.51),1,7,.018)
box("gun burgundy denied shell",(-1.10,-.61,1.90),(.21,.04,.29),1,7,.009)
rod("short barrel core",(-1.10,-.39,1.77),(-1.10,-.39,1.25),.066,2,7)
cyl("ported short muzzle",(-1.10,-.39,1.23),.108,.14,1,7,(0,0,1),16)
cyl("dark muzzle hole",(-1.10,-.39,1.145),.065,.008,1,7,(0,0,1),16)
box("amber gun status line",(-1.10,-.624,2.02),(.095,.014,.30),3,7,.004)
box("weapon side feed box",(-.87,-.32,1.86),(.13,.33,.28),1,7,.01)
rod("weapon grip seated in firing hand",(-.84,-.13,2.28),(-1.10,-.29,2.03),.055,2,7)
box("weapon hardened front spine",(-1.10,-.64,1.65),(.25,.13,.47),1,7,.009)
for z in (1.44,1.55,1.66):
    box("weapon vent band",(-1.10,-.716,z),(.21,.025,.028),2,7,.003)
box("side amber weapon battery",(-.856,-.35,1.82),(.035,.15,.20),3,7,.004)
for z in (1.77,2.10):
    bolt("gun receiver screw",(-1.294,-.54,z),(-1,0,0),7,.022)
# Offset the firearm toward the outer edge of the right hand so it remains
# legible from the front while staying close enough to read as held.
for weapon_piece in bpy.data.objects:
    if weapon_piece.type in ("MESH", "CURVE") and weapon_piece.get("part_id") == 7:
        weapon_piece.location.x -= .18
        weapon_piece.location.y -= .01

# Geometry wear survives semantic mesh export. Keep chips small enough that the
# burgundy and navy silhouette reads at distance; avoid broad random patches.
for pid,x_range,z_range,y,mid in (
    (0,(-.38,.34),(3.44,4.15),-.605,1),
    (2,(1.03,1.25),(3.84,4.34),-.446,0),
    (3,(-1.25,-1.03),(3.84,4.34),-.446,0),
    (4,(.30,.65),(.68,1.35),-.388,1),
    (5,(-.65,-.30),(.68,1.35),-.388,1),
):
    for _ in range(11):
        box("tiny bare edge scuff",(random.uniform(*x_range),y,random.uniform(*z_range)),
            (random.uniform(.012,.031),.007,random.uniform(.009,.029)),2,pid,0)


def convert(v):
    return (float(v.x), float(v.z), float(-v.y))


def game_position(v):
    return tuple(component * (1.8 / 5.35) for component in convert(v))


def export_isemesh(path):
    """Triangulate evaluated objects and gather by rigid part/material."""
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    groups = defaultdict(lambda: ([], []))
    triangles = 0
    for obj in sorted(bpy.data.objects, key=lambda o: o.name):
        if obj.type not in ("MESH", "CURVE") or "part_id" not in obj:
            continue
        pid = int(obj["part_id"])
        mid = int(obj["material_id"])
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
        if mesh is None:
            continue
        mesh.calc_loop_triangles()
        triangles += len(mesh.loop_triangles)
        world = evaluated.matrix_world
        normal_matrix = world.to_3x3().inverted().transposed()
        pivot = Vector(PARTS[pid][1])
        vertices, indices = groups[(pid, mid)]
        uv_layer = mesh.uv_layers.active.data if mesh.uv_layers.active else None
        for tri in mesh.loop_triangles:
            for loop_index in tri.loops:
                loop = mesh.loops[loop_index]
                vert = mesh.vertices[loop.vertex_index]
                pos = world @ vert.co
                normal = (normal_matrix @ loop.normal).normalized()
                uv = uv_layer[loop_index].uv if mid == 0 and uv_layer else (0., 0.)
                if mid == 0:
                    uv = (.002 + float(uv[0])*.496 + mid*.5, float(uv[1]))
                indices.append(len(vertices))
                # Evaluated bevel/weighted-normal float noise varies by a few
                # ULPs between Blender invocations. Quantize before writing so
                # the packaged mesh has a reproducible checksum.
                vertices.append(tuple(round(float(component), 4) for component in
                    (*game_position(pos), *convert(normal)))
                    + tuple(round(float(component), 3) for component in uv))
        evaluated.to_mesh_clear()
    assert len(groups) <= 32, f"submesh budget exceeded: {len(groups)}"
    with open(path, "wb") as out:
        out.write(b"ISEMESH1")
        out.write(struct.pack("<I", len(groups)))
        for (pid, mid), (vertices, indices) in sorted(groups.items()):
            out.write(struct.pack("<II3fII",pid,mid,*game_position(Vector(PARTS[pid][1])),len(vertices),len(indices)))
            for vertex in vertices:
                out.write(struct.pack("<8f",*vertex))
            for index in indices:
                out.write(struct.pack("<I",index))
    print(f"OFFICER_EXPORT_SUBMESHES={len(groups)} TRIANGLES={triangles} VERTICES={sum(len(v[0]) for v in groups.values())} BYTES={path.stat().st_size}")


export_isemesh(MESH_DIR / "officer_reference.isemesh")
if "--export-only" in sys.argv:
    raise SystemExit(0)

# Neutral four-angle review; stage and lamps remain in .blend, never in export.
floor = mat("neutral photo studio",(.28,.28,.27),0,.82)
bpy.ops.mesh.primitive_cube_add(size=1,location=(0,0,-.10))
stage=bpy.context.object
stage.name="review stage floor"
stage.dimensions=(40,40,.18)
bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
stage.data.materials.append(floor)
world=bpy.context.scene.world
world.use_nodes=True
world.node_tree.nodes["Background"].inputs["Color"].default_value=(.34,.34,.33,1)
world.node_tree.nodes["Background"].inputs["Strength"].default_value=.7


def area(name,pos,power,size):
    data=bpy.data.lights.new(name,"AREA")
    data.energy=power
    data.shape="DISK"
    data.size=size
    obj=bpy.data.objects.new(name,data)
    bpy.context.collection.objects.link(obj)
    obj.location=pos
    obj.rotation_euler=(Vector((0,0,2.8))-obj.location).to_track_quat("-Z","Y").to_euler()


area("neutral upper key",(-4,-5,8),610,5)
area("forward soft fill",(4,-2,6),330,5)
area("rear edge light",(1,5,7),470,4)
cam_data=bpy.data.cameras.new("four-view camera")
camera=bpy.data.objects.new("four-view camera",cam_data)
bpy.context.collection.objects.link(camera)
bpy.context.scene.camera=camera
cam_data.type="ORTHO"
cam_data.ortho_scale=6.4
scene=bpy.context.scene
scene.render.engine="BLENDER_EEVEE"
scene.render.resolution_x=680
scene.render.resolution_y=900
scene.render.resolution_percentage=100
scene.render.image_settings.file_format="PNG"
scene.view_settings.view_transform="AgX"
for name,pos in {
    "front":(0,-13,2.8),
    "left_side":(-13,0,2.8),
    "back":(0,13,2.8),
    "right_side":(13,0,2.8),
}.items():
    camera.location=pos
    camera.rotation_euler=(Vector((0,0,2.72))-camera.location).to_track_quat("-Z","Y").to_euler()
    scene.render.filepath=str(RENDER_DIR/f"{name}.png")
    bpy.ops.render.render(write_still=True)

camera.location=(0,-13,2.8)
camera.rotation_euler=(Vector((0,0,2.72))-camera.location).to_track_quat("-Z","Y").to_euler()
bpy.ops.wm.save_as_mainfile(filepath=str(MODEL_DIR/"officer_reference.blend"))
print("OFFICER_MODEL_PARTS="+",".join(PARTS[i][0] for i in range(8)))

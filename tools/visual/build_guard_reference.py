"""Build an eight-part Guard study from the four-view concept sheet.

Run from the repository root:
  blender -b --factory-startup --python tools/visual/build_guard_reference.py

To regenerate only the runtime mesh after exporter changes:
  blender -b --factory-startup --python tools/visual/build_guard_reference.py -- --export-only

Produces a .blend, four orthographic review PNGs, and the ISEMESH1 runtime
geometry. Materials in the export are semantic IDs; Blender's procedural paint
wear is only a visual study and is not baked into the exported mesh.
"""

from collections import defaultdict
import math
from pathlib import Path
import random
import struct
import sys

import bpy
from mathutils import Matrix, Vector


ROOT = Path(__file__).resolve().parents[2]
MODEL_DIR = ROOT / "assets/visual/models"
EXPORT_DIR = MODEL_DIR / "guard_reference"
RENDER_DIR = ROOT / "visual/validation/guard_reference_blender"
MODEL_DIR.mkdir(parents=True, exist_ok=True)
EXPORT_DIR.mkdir(parents=True, exist_ok=True)
RENDER_DIR.mkdir(parents=True, exist_ok=True)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
random.seed(6107)

# Blender: X right, Y depth (front is -Y), Z up.
# Runtime: X right, Y up, Z front. Conversion is (x, z, -y).
PARTS = {
    0: ("torso", (0, 0, 3.48)),
    1: ("head", (0, -.02, 4.47)),
    2: ("left_arm", (.95, -.01, 4.08)),
    3: ("right_arm", (-.95, -.01, 4.08)),
    4: ("left_leg", (.46, 0, 2.58)),
    5: ("right_leg", (-.46, 0, 2.58)),
    6: ("back", (0, .48, 3.85)),
    7: ("rifle", (0, -.73, 3.19)),
    8: ("left_forearm", (1.12, -.01, 3.15)),
    9: ("right_forearm", (-1.12, -.01, 3.15)),
}


def mat(name, color, metallic=0.0, roughness=.7, emit=0.0, wear=False):
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
    if wear:
        noise = n.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 85
        noise.inputs["Detail"].default_value = 4
        noise.inputs["Roughness"].default_value = .78
        ramp = n.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position = .36
        ramp.color_ramp.elements[0].color = (*color, 1)
        ramp.color_ramp.elements[1].position = .72
        ramp.color_ramp.elements[1].color = (.07, .066, .056, 1)
        ramp.color_ramp.elements.new(.59).color = (color[0]*.68, color[1]*.67, color[2]*.64, 1)
        l.new(noise.outputs["Fac"], ramp.inputs["Fac"])
        l.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
        bump = n.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = .08
        bump.inputs["Distance"].default_value = .01
        l.new(noise.outputs["Fac"], bump.inputs["Height"])
        l.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return m


# Export material IDs: 0 tan, 1 graphite, 2 steel, 3 amber, 4 rubber, 5 olive.
materials = {
    0: mat("worn sand ceramic armor", (.52, .48, .36), .42, .77, wear=True),
    1: mat("graphite structural frame", (.048, .049, .047), .55, .61),
    2: mat("scratched exposed steel", (.22, .22, .20), .73, .46),
    3: mat("amber optical sensor", (.72, .30, .006), .12, .24, emit=2.9),
    4: mat("black flexible seal", (.023, .023, .021), .08, .9),
    5: mat("weathered olive armor and canvas", (.24, .25, .19), .25, .83, wear=True),
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


def attach(obj, part_id, material_id):
    obj.data.materials.append(materials[material_id])
    obj["part_id"] = part_id
    obj["material_id"] = material_id
    obj.parent = roots[part_id]
    obj.matrix_parent_inverse = roots[part_id].matrix_world.inverted()
    return obj


def box(name, loc, dims, mid, pid, bevel=.018, rotation=None):
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
    mod.width = min(.016, radius*.12)
    mod.segments = 1
    obj.modifiers.new("weighted face normals", "WEIGHTED_NORMAL")
    return attach(obj, pid, mid)


def pipe(name, points, radius, mid, pid):
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.resolution_u = 8
    curve.bevel_depth = radius
    curve.bevel_resolution = 2
    spline = curve.splines.new("POLY")
    spline.points.add(len(points)-1)
    for p, xyz in zip(spline.points, points):
        p.co = (*xyz, 1)
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    return attach(obj, pid, mid)


def plate_xz(name, outline, y, depth, mid, pid, bevel=.018):
    """Angled plate outlined in the front plane and extruded through depth."""
    n = len(outline)
    vertices = [(x, y-depth/2, z) for x, z in outline]
    vertices += [(x, y+depth/2, z) for x, z in outline]
    faces = [tuple(reversed(range(n))), tuple(range(n, 2*n))]
    faces += [(i, (i+1)%n, (i+1)%n+n, i+n) for i in range(n)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    if bevel:
        mod = obj.modifiers.new("plate lip", "BEVEL")
        mod.width = bevel
        mod.segments = 1
        mod.affect = "EDGES"
        obj.modifiers.new("weighted normals", "WEIGHTED_NORMAL")
    return attach(obj, pid, mid)


def rod(name, a, b, radius, mid, pid, sides=12):
    direction = Vector(b)-Vector(a)
    center = (Vector(a)+Vector(b))*.5
    return cyl(name, center, radius, direction.length, mid, pid, direction, sides)


def bolt(name, pos, axis, pid, radius=.030):
    cyl(name+" dark washer", pos, radius*1.44, .012, 1, pid, axis, 10)
    outer = Vector(pos)+Vector(axis)*.009
    cyl(name+" eight-sided head", outer, radius, .017, 2, pid, axis, 8)


# Torso: open dark waist/shoulder mechanics and separate sand chest plates.
box("central steel torso chassis", (0, .04, 3.63), (1.67, .74, 1.35), 1, 0, .10)
box("deep lower abdomen", (0, -.22, 3.16), (1.20, .54, .48), 1, 0, .045)
plate_xz("angular upper breastplate", [(-.74,4.12),(-.57,4.29),(.57,4.29),(.74,4.12),(.57,3.66),(-.57,3.66)], -.44, .11, 0, 0, .028)
plate_xz("olive sternum bridge", [(-.35,3.90),(.35,3.90),(.48,3.53),(.30,3.30),(-.30,3.30),(-.48,3.53)], -.54, .11, 5, 0, .018)
for side in (-1,1):
    plate_xz("sand segmented side chest", [(side*.66,4.24),(side*.85,4.16),(side*.91,3.66),(side*.76,3.35),(side*.53,3.48)], -.39, .11, 0, 0)
    box("external chest webbing rail", (side*.64,-.55,3.69), (.15,.14,.98), 1, 0, .02)
    for z in (3.98,3.68,3.39):
        box("webbing retention clip", (side*.64,-.65,z), (.17,.10,.11), 2, 0, .008)
    cyl("waist axle", (side*.63,.01,2.91), .20, .25, 2, 0, (1,0,0))
    for z in (3.48,4.02):
        bolt("chest stamped fastener", (side*.55,-.625,z), (0,-1,0), 0)
box("pelvis suspension frame", (0,0,2.72), (1.22,.66,.47), 1, 0, .045)
plate_xz("crotch guard", [(-.37,2.91),(.37,2.91),(.28,2.44),(-.28,2.44)], -.38, .11, 0, 0)
for x in (-.37, 0, .37):
    box("low front ammunition pouch", (x,-.62,3.22), (.29,.21,.39), 1, 0, .02)
    box("ammunition pouch lid", (x,-.75,3.36), (.30,.07,.12), 2, 0, .009)
    box("ammunition pouch strap", (x,-.79,3.17), (.055,.035,.25), 4, 0, .003)
for x in (-.72,.72):
    box("waist belt latch", (x,-.37,2.93), (.22,.16,.19), 2, 0, .015)
# The concept has a load-bearing tactical harness, not a plain chest shell.
# Keep each pouch separate so the dark seams remain legible at game distance.
for side in (-1, 1):
    x = side*.49
    box("shoulder load strap dark backing", (x,-.625,3.93), (.24,.10,.64),1,0,.014)
    for z in (3.78,4.04,4.30):
        box("shoulder strap steel clasp", (x,-.691,z), (.26,.045,.095),2,0,.007)
    box("side chest layered ballistic tile", (side*.75,-.506,3.72),(.25,.13,.50),5,0,.023)
    box("side chest tile dark underlip", (side*.77,-.581,3.48),(.27,.045,.08),1,0,.006)
    for z in (3.51,3.81):
        box("side torso small cargo cell",(side*.94,-.31,z),(.20,.32,.24),5,0,.018)
        box("side cargo cell fastener",(side*1.053,-.34,z+.06),(.022,.20,.05),2,0,.004)
for x in (-.42,-.14,.14,.42):
    box("front ceramic magazine pouch",(x,-.68,3.59),(.235,.20,.37),5,0,.018)
    box("magazine pouch black flap",(x,-.792,3.72),(.245,.039,.105),1,0,.007)
    box("magazine pouch vertical release",(x,-.82,3.59),(.045,.034,.20),2,0,.004)
    box("magazine pouch bottom seam",(x,-.797,3.43),(.20,.035,.035),1,0,.003)
for x in (-.66,.66):
    rod("harness outer suspension",(x,-.65,4.31),(x,-.69,3.18),.043,1,0)
    bolt("harness chest buckle",(x,-.716,3.67),(0,-1,0),0,.029)
box("central chest equipment panel",(0,-.608,4.04),(.67,.10,.25),5,0,.015)
for x in (-.24,0,.24):
    box("chest panel recessed fastener",(x,-.673,4.06),(.10,.021,.09),1,0,.004)

# Head has a protected single wide optic, brow, cheek plates, side ear motors.
box("neck bellows", (0,0,4.38), (.48,.47,.25), 4, 1, .02)
box("helmet lower frame", (0,-.05,4.77), (.72,.67,.68), 1, 1, .055)
plate_xz("helmet sand crown", [(-.35,5.15),(-.24,5.28),(.24,5.28),(.35,5.15),(.33,4.99),(-.33,4.99)], -.12, .66, 0, 1, .03)
plate_xz("helmet upper face brow", [(-.41,5.00),(.41,5.00),(.37,4.84),(-.37,4.84)], -.45, .16, 0, 1, .015)
box("visor graphite recess", (0,-.536,4.81), (.62,.08,.25), 1, 1, .014)
box("amber inset visor", (0,-.584,4.83), (.45,.021,.15), 3, 1, .018)
box("visor dark central pupil", (0,-.602,4.83), (.073,.012,.09), 1, 1, .006)
for side in (-1,1):
    plate_xz("helmet sand jaw cheek", [(side*.29,4.76),(side*.42,4.79),(side*.36,4.51),(side*.20,4.52)], -.47, .13, 0, 1, .012)
    cyl("ear acoustic ring", (side*.43,-.02,4.84), .20, .13, 2, 1, (1,0,0), 20)
    cyl("ear central acoustic sensor", (side*.51,-.02,4.84), .115, .025, 1, 1, (1,0,0), 18)
    bolt("crown face screw", (side*.24,-.468,5.12), (0,-1,0), 1, .022)
box("front chin guard", (0,-.50,4.56), (.34,.15,.17), 0, 1, .014)
for x in (-.12,0,.12):
    box("chin intake slat", (x,-.583,4.58), (.025,.015,.085), 1, 1, .002)


def leg(side, pid):
    x = side*.49
    cyl("hip motor bearing", (x,0,2.55), .29,.34,2,pid,(1,0,0),20)
    box("thigh internal actuator", (x,.02,2.04), (.44,.47,.80),1,pid,.045)
    plate_xz("thigh sand armor", [(x-.24,2.43),(x+.24,2.43),(x+.30,1.88),(x+.21,1.67),(x-.23,1.67),(x-.29,1.94)], -.34,.13,0,pid,.025)
    box("upper thigh inner olive lip", (x-side*.20,-.26,2.13),(.11,.21,.55),5,pid,.015)
    cyl("knee large visible axle", (x,.01,1.57), .235,.56,2,pid,(1,0,0),18)
    cyl("knee outer dark cavity", (x+side*.30,.01,1.57), .14,.025,1,pid,(1,0,0),18)
    box("shin gunmetal rail", (x,.06,1.03), (.40,.48,.91),1,pid,.037)
    plate_xz("large faceted shin guard", [(x-.23,1.42),(x+.23,1.42),(x+.28,1.12),(x+.17,.57),(x-.17,.57),(x-.28,1.10)], -.30,.13,0,pid,.026)
    box("side shin olive strip", (x+side*.28,.01,1.02),(.10,.48,.75),5,pid,.014)
    plate_xz("rear thigh return armor",[(x-.22,2.35),(x+.22,2.35),(x+.20,1.83),(x-.20,1.83)],.31,.11,0,pid,.021)
    plate_xz("rear ankle shin shell",[(x-.19,1.31),(x+.19,1.31),(x+.16,.65),(x-.16,.65)],.31,.11,0,pid,.022)
    cyl("ankle exposed multi-ring bearing", (x,.02,.48), .18,.46,2,pid,(1,0,0),16)
    box("boot dark framework", (x,-.24,.22),(.60,.95,.38),1,pid,.04)
    box("rear heel forged plate",(x,.23,.23),(.53,.21,.30),2,pid,.022)
    for height in (.83,1.04,1.25):
        box("shin rear actuator tie",(x,.39,height),(.29,.055,.055),2,pid,.006)
    plate_xz("boot toe tan armor", [(x-.30,.36),(x+.30,.36),(x+.33,.15),(x-.33,.15)],-.68,.35,0,pid,.023)
    for toe in (-.19,0,.19):
        box("segmented steel boot toe",(x+toe,-.76,.105),(.17,.23,.16),2,pid,.012)
    for z in (.77,1.29,1.85,2.31):
        bolt("thigh shin armor screw",(x-side*.17,-.413,z),(0,-1,0),pid,.026)
    # Staggered armor lips and exposed actuator links break up the former
    # featureless blocks without adding collision bulk to the feet.
    for z in (1.01,1.29,1.94,2.20):
        box("leg front overlapping plate lip",(x,-.419,z),(.39,.036,.065),1,pid,.004)
        for dx in (-.17,.17):
            bolt("leg plate edge rivet",(x+dx,-.449,z),(0,-1,0),pid,.016)
    rod("shin front hydraulic track",(x+side*.11,-.405,1.30),(x+side*.11,-.405,.69),.032,2,pid)
    box("knee cap dark inset",(x,-.284,1.55),(.29,.048,.19),1,pid,.015)
    box("knee cap olive armor",(x,-.314,1.58),(.25,.045,.14),5,pid,.012)
    for z in (1.78,2.02,2.25):
        box("outer thigh pouch webbing",(x+side*.51,-.21,z),(.055,.10,.055),1,pid,.003)
    rod("exposed rear leg piston",(x+side*.12,.29,1.39),(x+side*.13,.31,.67),.055,2,pid)
    pipe("rear hose",[(x-side*.11,.31,2.30),(x-side*.13,.39,1.78),(x-side*.17,.34,1.41)],.033,4,pid)
    # Square modular supply satchels strapped to the outside of each thigh.
    for z in (2.24,2.00,1.76):
        box("thigh side utility pouch",(x+side*.40,-.01,z),(.18,.33,.19),5,pid,.015)
        box("thigh pouch dark lid",(x+side*.51,-.01,z+.07),(.03,.34,.08),1,pid,.004)


leg(1,4)
leg(-1,5)


def arm(side, pid):
    x=side*1.02
    forearm_pid=8 if side == 1 else 9
    cyl("shoulder universal joint",(x,0,4.08),.29,.36,2,pid,(1,0,0),20)
    box("shoulder inner bellows",(x+side*.13,-.03,3.99),(.39,.54,.59),4,0,.04)
    plate_xz("large angular shoulder pad",[(x-side*.18,4.37),(x+side*.22,4.40),(x+side*.40,4.17),(x+side*.32,3.83),(x-side*.10,3.78)],-.07,.77,0,0,.033)
    box("shoulder olive side insert",(x+side*.32,.03,4.10),(.10,.65,.36),5,0,.012)
    box("upper arm exposed steel rail",(x+side*.07,0,3.56),(.30,.39,.67),2,pid,.025)
    box("upper arm tan inset plate",(x+side*.12,-.25,3.59),(.27,.11,.54),0,pid,.023)
    cyl("elbow open rotary hinge",(x+side*.10,-.01,3.15),.19,.39,1,pid,(1,0,0),18)
    cyl("outer elbow steel ring",(x+side*.29,-.01,3.15),.14,.034,2,pid,(1,0,0),18)
    # Brought slightly inward; gloved hands visibly support the weapon.
    fx=x-side*.22
    box("forearm mechanism",(fx,-.30,2.84),(.34,.49,.51),1,forearm_pid,.025)
    plate_xz("sand forearm gauntlet",[(fx-.18,3.08),(fx+.18,3.08),(fx+.20,2.61),(fx-.18,2.61)],-.58,.16,0,forearm_pid,.024)
    cyl("wrist bearing",(fx,-.51,2.62),.13,.20,2,forearm_pid,(0,0,1),14)
    box("hand around rifle",(fx,-.78,2.53),(.29,.24,.19),1,forearm_pid,.021)
    for z in (3.90,4.27,2.80):
        bolt("arm armor fastening",(x+side*.05,-.465,z),(0,-1,0),
             forearm_pid if z < 3.15 else pid,.027)
    pipe("upper arm protected cable",[(x+side*.24,.27,4.17),(x+side*.30,.31,3.61),(x+side*.20,.22,3.17)],.028,4,pid)
    box("shoulder top overlapping armor lip",(x+side*.04,-.16,4.41),(.47,.60,.075),5,pid,.015)
    for z in (3.50,3.72,3.94):
        box("upper arm external rib",(x+side*.11,-.292,z),(.29,.039,.055),1,pid,.004)
    box("forearm side inset recess",(fx+side*.19,-.51,2.84),(.035,.15,.36),1,pid,.004)
    for z in (2.69,2.91):
        box("forearm side clasp",(fx+side*.22,-.55,z),(.036,.11,.065),2,pid,.004)


arm(1,2)
arm(-1,3)

# Rear supply backpack: high rectangular mass, side canisters, center door,
# exposed service ribs, antenna on the right when seen from behind.
box("backpack dark load frame",(0,.55,3.68),(1.31,.34,1.46),1,6,.046)
box("backpack main sand panel",(0,.77,3.71),(.82,.16,1.37),0,6,.024)
box("backpack overlapping olive lower panel",(0,.875,3.27),(.70,.10,.37),5,6,.020)
box("backpack center machined spine",(0,.91,3.82),(.18,.12,.96),2,6,.012)
for side in (-1,1):
    x=side*.60
    box("vertical olive supply module",(x,.81,3.82),(.31,.32,1.42),5,6,.026)
    for z in (3.36,3.70,4.04,4.38):
        box("supply module horizontal steel band",(x,1.00,z),(.33,.07,.065),2,6,.009)
    for z in (3.36,4.10):
        bolt("back supply screw",(side*.31,.875,z),(0,1,0),6,.029)
    pipe("waist pack cable",[(side*.41,.71,3.12),(side*.53,.56,2.94),(side*.61,.33,2.84)],.037,4,6)
box("backpack central graphite lock",(0,.89,3.69),(.49,.07,.30),1,6,.016)
for x in (-.28,.28):
    box("backpack latch strap",(x,.95,3.69),(.075,.06,.47),1,6,.009)
    box("backpack latch clasp",(x,.995,3.71),(.11,.035,.11),2,6,.007)
box("rear warning label",(0,.93,3.85),(.37,.016,.055),2,6,.003)
for x in (-.20,-.10,0,.10,.20):
    box("rear vent slat",(x,.88,4.22),(.045,.032,.21),1,6,.002)
# The concept's pack projects behind the body as three latched supply cassettes
# and has a visible service bay instead of one uninterrupted rear panel.
for x in (-.39,0,.39):
    box("rear upper supply cassette",(x,.998,4.05),(.34,.18,.51),0,6,.018)
    box("supply cassette upper black hinge",(x,1.101,4.26),(.32,.045,.055),1,6,.004)
    box("supply cassette lower latch",(x,1.110,3.85),(.11,.036,.09),2,6,.006)
    for z in (3.90,4.16):
        bolt("rear cassette rivet",(x-.10,1.104,z),(0,1,0),6,.017)
box("rear lower access door",(0,.997,3.35),(.67,.17,.43),1,6,.02)
box("rear lower door inset",(0,1.088,3.36),(.51,.025,.28),5,6,.009)
for x in (-.68,.68):
    box("rear side battery shield",(x,1.015,3.75),(.25,.18,1.25),0,6,.020)
    for z in (3.27,3.67,4.06):
        box("rear battery shield seam",(x,1.114,z),(.24,.024,.043),1,6,.003)
        bolt("rear battery lock",(x,1.132,z+.12),(0,1,0),6,.017)
for x in (-.24,.24):
    pipe("rear protected power conduit",[(x,.93,3.48),(x,1.13,3.06),(x*.8,.72,2.77)],.045,1,6)
cyl("antenna root socket",(.55,.62,4.58),.09,.17,2,6,(0,0,1),12)
rod("long radio antenna",(.55,.62,4.62),(.55,.62,5.45),.031,1,6)
cyl("antenna tip",(.55,.62,5.45),.040,.055,2,6,(0,0,1),10)

# Long, diagonally carried rifle, from rear stock (left) to muzzle (right).
# Local weapon coordinates: t along length, s across its side, h thickness.
RIFLE_A=Vector((-.93,-.48,3.79))
RIFLE_B=Vector((1.44,-1.43,2.67))
D=(RIFLE_B-RIFLE_A).normalized()
SIDE=Vector((0,1,0))
UP=D.cross(SIDE).normalized()


def weapon_point(t, side=0, up=0):
    return RIFLE_A+D*t+SIDE*side+UP*up


def weapon_box(name,t,length,width,height,mid, side=0,up=0,bevel=.01):
    obj=box(name,weapon_point(t,side,up),(length,width,height),mid,7,bevel)
    # Local cube X runs along the rifle, Y across depth, Z in face plane.
    basis=Matrix(((D.x,SIDE.x,UP.x),(D.y,SIDE.y,UP.y),(D.z,SIDE.z,UP.z)))
    obj.rotation_euler=basis.to_euler()
    return obj


weapon_box("rear adjustable shoulder stock",.20,.48,.22,.26,1)
weapon_box("stock sand upper cheek",.30,.55,.25,.11,0,up=.10)
weapon_box("stock top rail",.26,.66,.12,.055,2,up=.21)
weapon_box("stock butt recoil plate",-.035,.09,.27,.32,2)
for t in (.04,.17,.30):
    weapon_box("stock skeletal relief",t,.055,.245,.05,5,up=-.10)
weapon_box("steel receiver central",1.04,1.12,.26,.26,1)
weapon_box("sand faceted receiver upper",1.02,.90,.31,.13,0,up=.16)
weapon_box("olive receiver lower",1.12,.61,.32,.105,5,up=-.15)
weapon_box("receiver long tactical side plate",1.10,1.00,.045,.19,0,side=-.174,up=.02)
weapon_box("receiver ejection cavity",1.07,.39,.026,.085,1,side=-.205,up=.055)
for t in (.67,.82,.97,1.12,1.27,1.42):
    weapon_box("receiver side retaining screw",t,.028,.020,.027,2,side=-.225,up=.13,bevel=.003)
weapon_box("forward vented handguard",1.88,.73,.27,.22,5)
weapon_box("handguard dark upper rail",1.84,.85,.27,.055,1,up=.17)
for i in range(6):
    weapon_box("handguard rail slot",1.52+i*.12,.04,.292,.027,1,up=.065,bevel=0)
    weapon_box("handguard side vent",1.52+i*.12,.063,.020,.057,1,side=-.15,up=-.020,bevel=.002)
    weapon_box("handguard side vent rim",1.52+i*.12,.070,.018,.018,2,side=-.164,up=.045,bevel=.002)
weapon_box("barrel shroud",2.45,.52,.13,.13,1)
weapon_box("underbarrel support rail",2.11,1.14,.11,.075,2,up=-.155)
rod("extended steel barrel",weapon_point(2.60),weapon_point(3.30),.052,2,7)
rod("muzzle brake",weapon_point(3.24),weapon_point(3.48),.084,1,7)
for i in (-1,1):
    for t in (3.32,3.42):
        weapon_box("muzzle brake side port",t,.055,.016,.047,2,side=i*.086,bevel=0)
weapon_box("rifle trigger guard",.89,.23,.18,.08,2,up=-.25)
weapon_box("rifle grip",.78,.21,.19,.36,1,up=-.28)
weapon_box("curved box magazine",1.25,.29,.19,.51,1,up=-.34)
weapon_box("magazine sand side armor",1.26,.23,.22,.30,0,side=-.02,up=-.31)
weapon_box("upper optical scope rail",1.00,.65,.11,.08,2,up=.31)
rod("cylindrical optical scope",weapon_point(.86,0,.44),weapon_point(1.38,0,.44),.11,1,7)
cyl("scope amber objective",weapon_point(1.40,0,.44),.085,.016,3,7,D,16)
for t in (.86,1.30):
    cyl("scope heavy mounting clamp",weapon_point(t,0,.44),.13,.08,2,7,D,16)
weapon_box("side tactical charging handle",1.38,.17,.15,.06,2,side=-.25,up=.10)
weapon_box("forward laser range unit",2.04,.41,.13,.11,1,side=.19,up=.02)
cyl("range unit amber lens",weapon_point(2.26,.19,.02),.044,.02,3,7,D,14)
for t in (.64,1.04,1.45,1.80,2.10):
    weapon_box("rifle machined side fastener",t,.038,.045,.034,2,side=-.15,up=.05,bevel=.004)

# Paint wear is visible in Blender, but geometry also includes deterministic
# exposed edge nicks which survive export under the steel material ID.
for pid, center_x, z_range, y in ((0,0,(3.7,4.22),-.505),
                                  (4,.49,(.68,1.35),-.386),
                                  (5,-.49,(.68,1.35),-.386)):
    for i in range(16):
        x=center_x+random.uniform(-.20,.20)
        z=random.uniform(*z_range)
        box("small exposed paint edge chip",(x,y,z),
            (random.uniform(.012,.035),.008,random.uniform(.009,.033)),2,pid,0)


def convert(v):
    return (float(v.x),float(v.z),float(-v.y))


def game_position(v):
    return tuple(component * (1.8 / 5.35) for component in convert(v))


def export_isemesh(path):
    """Triangulate evaluated meshes, collect by rigid part and semantic material."""
    bpy.context.view_layer.update()
    depsgraph=bpy.context.evaluated_depsgraph_get()
    groups=defaultdict(lambda: ([], []))
    vertex_maps={}
    polygons=0
    for obj in sorted(bpy.data.objects,key=lambda o:o.name):
        if obj.type not in ("MESH","CURVE") or "part_id" not in obj:
            continue
        pid=int(obj["part_id"])
        mid=int(obj["material_id"])
        # Nearby graphite and rubber are one dark channel at game distance.
        # This keeps the full Blender materials editable while reducing GPU
        # draw groups for the Quadro K2100M.
        if mid == 4:
            mid = 1
        if pid == 0 and mid == 5:
            mid = 0
        evaluated=obj.evaluated_get(depsgraph)
        mesh=evaluated.to_mesh(preserve_all_data_layers=True,depsgraph=depsgraph)
        if mesh is None:
            continue
        mesh.calc_loop_triangles()
        polygons+=len(mesh.loop_triangles)
        world=evaluated.matrix_world
        normal_matrix=world.to_3x3().inverted().transposed()
        pivot=Vector(PARTS[pid][1])
        verts,indices=groups[(pid,mid)]
        vertex_map=vertex_maps.setdefault((pid,mid),{})
        uv_layer=mesh.uv_layers.active.data if mesh.uv_layers.active else None
        for tri in mesh.loop_triangles:
            for loop_index in tri.loops:
                loop=mesh.loops[loop_index]
                vert=mesh.vertices[loop.vertex_index]
                pos=world @ vert.co
                normal=(normal_matrix @ loop.normal).normalized()
                # Runtime uses an albedo texture only for painted armor. UVs
                # emitted by Blender's bevel modifier fluctuate for unpainted
                # steel/trim, so give those batches a stable unused value.
                uv=uv_layer[loop_index].uv if mid == 0 and uv_layer else (0.0,0.0)
                # Evaluated bevel normals can differ by one float ULP between
                # Blender runs. Quantize before deduplication and serialization
                # so identical source geometry produces identical runtime bytes.
                record=tuple(round(float(value),5) for value in
                             (*game_position(pos),*convert(normal))) + \
                       tuple(round(float(value),2) for value in uv)
                index=vertex_map.get(record)
                if index is None:
                    index=len(verts)
                    vertex_map[record]=index
                    verts.append(record)
                indices.append(index)
        evaluated.to_mesh_clear()
    with open(path,"wb") as out:
        out.write(b"ISEMESH1")
        out.write(struct.pack("<I",len(groups)))
        for (pid,mid),(verts,indices) in sorted(groups.items()):
            out.write(struct.pack("<II3fII",pid,mid,*game_position(Vector(PARTS[pid][1])),len(verts),len(indices)))
            for v in verts:
                out.write(struct.pack("<8f",*v))
            for index in indices:
                out.write(struct.pack("<I",index))
    print(f"GUARD_EXPORT_SUBMESHES={len(groups)} TRIANGLES={polygons} VERTICES={sum(len(v[0]) for v in groups.values())} BYTES={path.stat().st_size}")


export_isemesh(EXPORT_DIR/"guard_reference.isemesh")
if "--export-only" in sys.argv:
    print("GUARD_EXPORT_ONLY=1")
    raise SystemExit(0)

# Neutral review stage. The floor is never exported.
floor=mat("neutral photo studio",(.28,.28,.27),.0,.82)
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


area("large neutral key",(-4,-5,8),950,5)
area("neutral fill",(4,-2,6),560,5)
area("back edge light",(1,5,7),680,4)
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
bpy.ops.wm.save_as_mainfile(filepath=str(MODEL_DIR/"guard_reference.blend"))
print("GUARD_MODEL_PARTS="+",".join(PARTS[i][0] for i in range(10)))

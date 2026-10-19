"""Build an eight-part Engineer study from the four-view concept sheet.

Run from the repository root:
  blender -b --factory-startup --python tools/visual/build_engineer_reference.py

Produces editable Blender geometry, four orthographic review images and an
ISEMESH1 runtime mesh. Procedural material wear is a Blender preview; chips and
layered mechanical details are actual geometry in the runtime export.
"""

from collections import defaultdict
import math
from pathlib import Path
import random
import struct
import sys

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
MODEL_DIR = ROOT / "assets/visual/models"
EXPORT_DIR = MODEL_DIR / "engineer_reference"
RENDER_DIR = ROOT / "visual/validation/engineer_reference_blender"
for path in (MODEL_DIR, EXPORT_DIR, RENDER_DIR):
    path.mkdir(parents=True, exist_ok=True)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
random.seed(82064)

# Blender axes: X right, Y depth (front is -Y), Z up.
# Runtime axes: X right, Y up, Z front. Conversion: (x, z, -y).
PARTS = {
    0: ("torso", (0, 0, 3.55)),
    1: ("head", (0, -.05, 4.56)),
    2: ("left_arm_repair", (1.02, 0, 4.13)),
    3: ("right_arm_rifle", (-1.02, 0, 4.13)),
    4: ("left_leg", (.50, 0, 2.57)),
    5: ("right_leg", (-.50, 0, 2.57)),
    6: ("diagnostic_backpack", (0, .54, 3.83)),
    7: ("repair_manipulator", (1.28, -.45, 2.75)),
}


def material(name, color, metallic=.0, roughness=.7, emission=0, worn=False):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if emission:
        bsdf.inputs["Emission Color"].default_value = (*color, 1)
        bsdf.inputs["Emission Strength"].default_value = emission
    if worn:
        noise = nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 70
        noise.inputs["Detail"].default_value = 4
        noise.inputs["Roughness"].default_value = .77
        ramp = nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position = .37
        ramp.color_ramp.elements[0].color = (*color, 1)
        ramp.color_ramp.elements[1].position = .75
        ramp.color_ramp.elements[1].color = (.055, .063, .057, 1)
        ramp.color_ramp.elements.new(.59).color = (color[0]*.66, color[1]*.60, color[2]*.54, 1)
        links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
        links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
        bump = nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = .075
        bump.inputs["Distance"].default_value = .011
        links.new(noise.outputs["Fac"], bump.inputs["Height"])
        links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


# Runtime semantic material IDs: teal, orange, steel, luminous amber, graphite,
# secondary trim. The preview uses the same broad palette as the reference.
M = {
    0: material("worn petrol teal armor", (.17, .36, .37), .48, .65, worn=True),
    1: material("worn warning orange armor", (.68, .27, .045), .50, .58, worn=True),
    2: material("scratched exposed steel", (.25, .25, .23), .73, .42),
    3: material("amber optical and repair light", (1.0, .39, .017), .18, .23, emission=3.1),
    4: material("deep graphite rubber and joints", (.035, .042, .041), .38, .75),
    5: material("warm industrial secondary trim", (.36, .29, .19), .53, .62, worn=True),
}

ROOTS = {}
for part_id, (name, pivot) in PARTS.items():
    obj = bpy.data.objects.new(name + " pivot", None)
    bpy.context.collection.objects.link(obj)
    obj.location = pivot
    obj.empty_display_type = "ARROWS"
    obj.empty_display_size = .15
    obj["part_id"] = part_id
    ROOTS[part_id] = obj


def attach(obj, pid, mid):
    # Keep the rigid-part/material batches within the portable 32-draw budget.
    # These channels are deliberately shared in both Blender and the export.
    if pid in (1, 4, 5) and mid == 2:
        mid = 4
    if pid == 7 and mid == 0:
        mid = 1
    obj.data.materials.append(M[mid])
    obj["part_id"] = pid
    obj["material_id"] = mid
    obj.parent = ROOTS[pid]
    obj.matrix_parent_inverse = ROOTS[pid].matrix_world.inverted()
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
        mod.segments = 2
        mod.affect = "EDGES"
        obj.modifiers.new("weighted normals", "WEIGHTED_NORMAL")
    return attach(obj, pid, mid)


def cyl(name, loc, radius, depth, mid, pid, axis=(0, 0, 1), sides=16):
    bpy.ops.mesh.primitive_cylinder_add(vertices=sides, radius=radius, depth=depth, location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler = Vector((0, 0, 1)).rotation_difference(Vector(axis)).to_euler()
    mod = obj.modifiers.new("machined rim", "BEVEL")
    mod.width = min(.014, radius*.12)
    mod.segments = 2
    obj.modifiers.new("weighted normals", "WEIGHTED_NORMAL")
    return attach(obj, pid, mid)


def rod(name, a, b, radius, mid, pid, sides=12):
    direction = Vector(b)-Vector(a)
    center = (Vector(a)+Vector(b))*.5
    return cyl(name, center, radius, direction.length, mid, pid, direction, sides)


def ring(name, loc, major, minor, mid, pid, axis=(0, 1, 0)):
    bpy.ops.mesh.primitive_torus_add(major_segments=24, minor_segments=8,
                                     location=loc, major_radius=major,
                                     minor_radius=minor)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler = Vector((0, 0, 1)).rotation_difference(Vector(axis)).to_euler()
    return attach(obj, pid, mid)


def cable(name, points, radius, mid, pid):
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.resolution_u = 8
    curve.bevel_depth = radius
    curve.bevel_resolution = 2
    spline = curve.splines.new("POLY")
    spline.points.add(len(points)-1)
    for point, xyz in zip(spline.points, points):
        point.co = (*xyz, 1)
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    return attach(obj, pid, mid)


def plate(name, outline, y, depth, mid, pid, bevel=.018):
    count = len(outline)
    verts = [(x, y-depth/2, z) for x, z in outline]
    verts += [(x, y+depth/2, z) for x, z in outline]
    faces = [tuple(reversed(range(count))), tuple(range(count, 2*count))]
    faces += [(i, (i+1)%count, (i+1)%count+count, i+count) for i in range(count)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    if bevel:
        mod = obj.modifiers.new("stamped plate edge", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        mod.affect = "EDGES"
        obj.modifiers.new("weighted normals", "WEIGHTED_NORMAL")
    return attach(obj, pid, mid)


def bolt(name, pos, axis, pid, radius=.026):
    cyl(name + " dark washer", pos, radius*1.42, .012, 4, pid, axis, 10)
    outer = Vector(pos)+Vector(axis)*.010
    cyl(name + " steel bolt", outer, radius, .018, 2, pid, axis, 8)


# Torso: broad teal industrial front, floating orange central service hatch,
# exposed dark waist and overlapping waist armor.
box("central chassis", (0,.02,3.64), (1.69,.75,1.35), 4,0,.095)
box("ribbed lower abdomen",(0,-.20,3.17),(1.30,.53,.51),2,0,.045)
plate("wide shoulder yoke",[(-.82,4.21),(-.63,4.35),(.63,4.35),(.82,4.21),(.68,3.91),(-.68,3.91)],-.39,.19,0,0,.030)
plate("left chest module",[(-.74,4.16),(-.20,4.15),(-.17,3.49),(-.57,3.46),(-.77,3.69)],-.54,.13,0,0,.025)
plate("right chest module",[(.21,4.16),(.75,4.16),(.77,3.70),(.57,3.46),(.17,3.49)],-.54,.13,0,0,.025)
plate("orange removable chest hatch",[(-.39,4.03),(.39,4.03),(.48,3.79),(.39,3.38),(-.39,3.38),(-.48,3.79)],-.64,.11,1,0,.024)
box("upper black carry handle",(0,-.725,4.02),(.55,.10,.08),4,0,.01)
for side in (-1, 1):
    x = side*.78
    box("chest removable side latch", (x,-.535,3.82),(.16,.09,.32),2,0,.012)
    box("waist exposed rib", (side*.43,-.47,3.17),(.17,.06,.42),4,0,.009)
    rod("chest exposed hydraulic line",(side*.71,-.45,3.54),
        (side*.81,-.43,4.08),.037,2,0)
    for z in (3.57, 3.70, 3.83):
        box("side chest recessed vent",(side*.66,-.615,z),(.13,.019,.026),4,0,.002)
for x in (-.25,.25):
    box("service hatch handle mount",(x,-.735,3.85),(.07,.10,.27),2,0,.007)
    bolt("chest hatch mounting screw",(x,-.715,3.45),(0,-1,0),0)
for side in (-1,1):
    box("outer lateral teal armor",(side*.79,-.20,3.74),(.25,.46,.87),0,0,.032)
    cyl("waist rotary motor",(side*.63,.04,2.94),.235,.30,2,0,(1,0,0),20)
    cable("orange hydraulic waist line",[(side*.64,-.31,3.71),(side*.77,-.44,3.30),(side*.67,-.26,2.99)],.039,1,0)
    for z in (3.64,4.08):
        bolt("side chest captive screw",(side*.60,-.615,z),(0,-1,0),0)
box("pelvis box frame",(0,0,2.73),(1.25,.66,.51),4,0,.05)
plate("orange lower belly guard",[(-.31,2.96),(.31,2.96),(.27,2.50),(-.27,2.50)],-.41,.14,1,0,.025)
for x in (-.47,0,.47):
    box("front waist clasp",(x,-.41,2.94),(.19,.13,.15),2,0,.013)

# Compact helmet with inset glowing amber face and visible mechanical ears.
box("neck hydraulic stem",(0,0,4.38),(.38,.38,.22),2,1,.022)
box("helmet graphite shell",(0,-.05,4.75),(.70,.63,.65),4,1,.055)
plate("teal crown armored cap",[(-.35,5.13),(-.26,5.27),(.26,5.27),(.35,5.13),(.29,4.98),(-.29,4.98)],-.08,.64,0,1,.025)
box("orange forehead warning strip",(0,-.418,5.04),(.54,.11,.12),1,1,.012)
box("visor deep graphite bezel",(0,-.40,4.81),(.56,.18,.43),4,1,.025)
box("single amber face sensor",(0,-.505,4.82),(.38,.015,.28),3,1,.018)
box("visor lower hood",(0,-.515,4.66),(.65,.14,.07),2,1,.011)
for x in (-.26,.26):
    box("visor steel vertical retainer",(x,-.515,4.82),(.046,.09,.35),2,1,.005)
    bolt("visor corner screw",(x,-.57,4.67),(0,-1,0),1,.016)
box("sensor lower steel chin",(0,-.472,4.57),(.48,.14,.15),2,1,.018)
for side in (-1,1):
    cyl("ear circular bearing",(side*.42,-.04,4.83),.19,.15,2,1,(1,0,0),18)
    cyl("ear dark inset",(side*.51,-.04,4.83),.12,.025,4,1,(1,0,0),16)
    box("small orange comms bracket",(side*.48,.07,5.12),(.18,.19,.29),1,1,.020)
    bolt("helmet top screw",(side*.23,-.436,5.11),(0,-1,0),1,.021)
rod("rear optic antenna",(-.54,.23,5.05),(-.54,.23,5.47),.033,2,1)


def leg(side, pid):
    x=side*.50
    cyl("large hip axle",(x,0,2.56),.31,.40,2,pid,(1,0,0),20)
    box("thigh actuator chassis",(x,.02,2.08),(.42,.49,.82),4,pid,.038)
    plate("long teal thigh panel",[(x-.23,2.40),(x+.23,2.40),(x+.29,1.91),(x+.20,1.66),(x-.20,1.66),(x-.29,1.93)],-.36,.16,0,pid,.025)
    plate("rear teal thigh panel",[(x-.21,2.38),(x+.21,2.38),(x+.20,1.82),(x-.20,1.82)],.34,.13,0,pid,.023)
    box("orange thigh bottom cuff",(x,-.465,1.75),(.47,.11,.16),1,pid,.015)
    box("thigh angled outer service cassette",(x+side*.30,-.05,2.05),(.16,.49,.50),0,pid,.020)
    for z in (1.88, 2.02, 2.16):
        box("thigh side vent slot",(x+side*.39,-.17,z),(.025,.22,.037),4,pid,.003)
    cyl("knee exposed motor",(x,.01,1.57),.25,.58,2,pid,(1,0,0),18)
    cyl("knee graphite cavity",(x+side*.31,.01,1.57),.145,.030,4,pid,(1,0,0),16)
    box("shin inner steel rail",(x,.02,1.00),(.39,.47,.89),2,pid,.035)
    plate("teal shin face",[(x-.22,1.37),(x+.22,1.37),(x+.27,1.07),(x+.19,.55),(x-.19,.55),(x-.27,1.06)],-.32,.15,0,pid,.024)
    plate("teal rear calf cover",[(x-.19,1.33),(x+.19,1.33),(x+.17,.62),(x-.17,.62)],.31,.12,0,pid,.022)
    cyl("ankle rotary motor",(x,0,.47),.18,.45,2,pid,(1,0,0),16)
    box("broad boot frame",(x,-.22,.21),(.65,.99,.39),4,pid,.045)
    box("orange boot collar",(x,-.45,.38),(.57,.35,.16),1,pid,.021)
    for toe in (-.21,0,.21):
        box("segmented machine toe",(x+toe,-.75,.12),(.19,.25,.19),2,pid,.014)
    for z in (.74,1.24,1.82,2.32):
        bolt("leg plate fixing",(x-side*.17,-.455,z),(0,-1,0),pid,.025)
    rod("exposed rear leg piston",(x+side*.11,.31,1.36),(x+side*.14,.32,.68),.051,2,pid)
    cable("rear ankle wiring",[(x-side*.18,.33,1.37),(x-side*.22,.44,1.04),(x-side*.13,.32,.65)],.028,4,pid)
    box("outer ankle orange armor",(x+side*.30,-.13,.47),(.12,.36,.28),1,pid,.015)
    for z in (.80, 1.02, 1.24):
        box("shin side mechanism rack",(x+side*.27,.07,z),(.13,.28,.055),4,pid,.007)
    cyl("large visible knee side bearing",(x+side*.34,.01,1.57),.12,.08,2,pid,(1,0,0),16)
    for toe in (-.21, 0, .21):
        box("toe front knuckle",(x+toe,-.90,.19),(.16,.10,.13),4,pid,.009)


leg(1,4)
leg(-1,5)


def arm(side,pid,repair):
    x=side*1.02
    cyl("shoulder universal joint",(x,0,4.13),.31,.38,2,pid,(1,0,0),20)
    box("shoulder bellows",(x+side*.14,0,4.03),(.39,.49,.63),4,pid,.036)
    plate("large teal shoulder pauldrons",[(x-side*.19,4.36),(x+side*.20,4.47),(x+side*.43,4.22),(x+side*.33,3.75),(x-side*.09,3.79)],-.08,.75,0,pid,.035)
    box("orange shoulder trim",(x+side*.35,-.13,4.03),(.11,.45,.38),1,pid,.015)
    box("upper arm steel piston cage",(x+side*.10,0,3.57),(.31,.41,.69),2,pid,.023)
    box("upper arm teal side plate",(x+side*.13,-.25,3.58),(.30,.12,.59),0,pid,.023)
    for z in (3.41,3.54,3.67):
        box("upper arm exposed rib",(x+side*.36,.04,z),(.055,.35,.065),4,pid,.006)
    rod("arm side hydraulic piston",(x+side*.32,.20,3.89),
        (x+side*.32,.20,3.24),.048,2,pid)
    cyl("elbow external motor",(x+side*.13,0,3.13),.205,.40,2,pid,(1,0,0),18)
    cyl("elbow dark center",(x+side*.34,0,3.13),.12,.036,4,pid,(1,0,0),16)
    box("forearm cage",(x+side*.16,-.10,2.80),(.32,.42,.60),4,pid,.025)
    plate("teal segmented forearm",[(x+side*.01,3.09),(x+side*.31,3.09),(x+side*.35,2.53),(x+side*.02,2.53)],-.35,.17,0,pid,.020)
    box("orange wrist latch",(x+side*.17,-.40,2.55),(.30,.10,.16),1,pid,.012)
    cyl("wrist axle",(x+side*.16,-.16,2.46),.13,.25,2,pid,(0,0,1),14)
    box("steel clamp hand",(x+side*.17,-.36,2.35),(.27,.32,.23),2,pid,.014)
    box("forearm orange side buckle",(x+side*.38,-.17,2.82),(.10,.26,.32),1,pid,.009)
    for z in (2.66,2.79,2.92):
        box("forearm inset mechanism slot",(x+side*.365,-.31,z),(.03,.075,.04),4,pid,.003)
    for z in (3.92,4.30,2.80):
        bolt("arm armor screw",(x+side*.11,-.470,z),(0,-1,0),pid,.026)
    cable("visible joint wiring",[(x+side*.25,.23,4.21),(x+side*.36,.31,3.66),(x+side*.25,.25,3.16)],.034,4,pid)
    if repair:
        cyl("technical elbow port",(x+side*.35,-.02,3.13),.085,.050,1,pid,(1,0,0),14)


arm(1,2,True)
arm(-1,3,False)

# Rifle on the right arm, visually separated from the repair manipulator.
# The reference holds it vertically alongside the thigh, so it travels with
# the right-arm rigid group instead of sharing part 7 with the repair tool.
box("rifle upper receiver",(-1.18,-.36,2.34),(.23,.35,.62),4,3,.023)
box("rifle teal receiver shell",(-1.24,-.56,2.38),(.23,.10,.54),0,3,.016)
box("rifle orange access plate",(-1.25,-.63,2.35),(.12,.045,.31),1,3,.008)
box("rifle lower magazine",(-1.18,-.39,1.91),(.20,.28,.36),4,3,.014)
box("rifle orange magazine foot",(-1.18,-.39,1.76),(.23,.31,.12),1,3,.011)
box("rifle upper shoulder shroud",(-1.18,-.37,2.81),(.25,.37,.30),0,3,.017)
rod("rifle long steel barrel",(-1.20,-.37,1.86),(-1.20,-.37,.98),.073,2,3)
cyl("rifle exposed barrel collar",(-1.20,-.37,1.53),.12,.14,1,3,(0,0,1),16)
cyl("rifle muzzle brake",(-1.20,-.37,.99),.103,.21,4,3,(0,0,1),14)
for z in (1.08, 1.13, 1.18):
    box("rifle muzzle vent",(-1.20,-.477,z),(.12,.014,.018),2,3,.002)
box("rifle side optic",(-1.22,-.59,2.68),(.17,.16,.18),2,3,.009)
box("rifle amber side display",(-1.22,-.686,2.68),(.10,.016,.08),3,3,.003)
for z in (2.12,2.31,2.52):
    box("rifle vented side slot",(-1.24,-.626,z),(.12,.018,.028),4,3,.002)

# Left manipulator / welding torch: forearm-hand chassis, orange coil and
# glowing nozzle.  It projects laterally to the right in the front view.
box("repair tool primary dark motor",(1.28,-.43,2.67),(.51,.47,.52),4,7,.036)
box("repair tool orange rectangular housing",(1.43,-.54,2.70),(.39,.19,.50),1,7,.027)
box("repair tool side service panel",(1.54,-.66,2.78),(.20,.035,.29),0,7,.013)
box("repair tool large front diagnostic case",(1.50,-.73,2.60),(.38,.20,.34),4,7,.024)
box("repair tool orange sloping nose",(1.65,-.77,2.52),(.31,.16,.25),1,7,.016)
box("repair tool amber status window",(1.44,-.837,2.67),(.13,.016,.075),3,7,.003)
cyl("repair tool large winding motor",(1.15,-.38,2.99),.23,.35,2,7,(1,0,0),20)
cyl("repair tool dark hub",(1.34,-.38,2.99),.115,.035,4,7,(1,0,0),18)
ring("repair tool copper motor ring",(1.36,-.38,2.99),.17,.030,1,7,(1,0,0))
for z in (2.43,2.58,2.73):
    box("repair tool side grip rib",(1.69,-.66,z),(.085,.15,.055),2,7,.006)
rod("repair tool steel lower rail",(1.49,-.44,2.58),(1.72,-.49,2.28),.079,2,7)
rod("repair tool lower return brace",(1.42,-.32,2.60),(1.80,-.39,2.29),.040,2,7)
rod("repair tool copper probe neck",(1.72,-.49,2.28),(1.88,-.52,2.08),.078,1,7)
cyl("repair tool concentric nozzle collar",(1.86,-.52,2.12),.12,.11,2,7,(.50,-.07,-.86),16)
cyl("repair tool illuminated nozzle",(1.91,-.53,2.05),.090,.13,3,7,(.50,-.07,-.86),12)
rod("short luminous repair contact",(1.95,-.54,1.98),(2.03,-.55,1.82),.024,3,7,10)
cable("tool orange outer cable",[(1.11,-.30,3.13),(1.42,-.28,3.05),(1.67,-.35,2.72),(1.74,-.47,2.31)],.044,1,7)
cable("tool dark black return hose",[(1.03,-.26,2.95),(1.22,-.17,2.73),(1.53,-.22,2.50),(1.77,-.42,2.28)],.039,4,7)
cable("tool thick looped power umbilical",[(.79,.55,3.69),(.94,.82,3.22),
      (1.17,.65,2.98),(1.42,.21,2.78),(1.63,-.18,2.55)],.056,1,7)
for x in (1.27,1.38,1.49):
    box("repair tool copper clamp tooth",(x,-.85,2.42),(.066,.10,.19),2,7,.006)
for z in (2.57,2.82):
    bolt("tool outer captive fastener",(1.55,-.689,z),(0,-1,0),7,.027)

# Tall diagnostic backpack with two asymmetrical status columns, a recessed
# black ventilation field, prominent copper cable reel and conduit runs.
box("backpack welded load frame",(0,.60,3.79),(1.38,.56,1.57),4,6,.055)
box("backpack teal side return",(-.54,.84,3.86),(.34,.31,1.42),0,6,.025)
box("backpack upper orange cabinet",(0,1.00,4.18),(.98,.27,.87),1,6,.027)
box("backpack central black access door",(-.20,1.165,4.20),(.49,.06,.61),4,6,.016)
box("backpack orange vertical status bezel",(.36,1.177,4.21),(.23,.06,.74),1,6,.010)
box("backpack long amber power indicator",(.36,1.215,4.21),(.093,.019,.60),3,6,.006)
box("backpack outer black vent housing",(-.52,1.14,4.10),(.29,.13,.65),4,6,.014)
for z in (3.88,3.99,4.10,4.21,4.32):
    box("backpack grille horizontal bar",(-.52,1.222,z),(.22,.032,.039),2,6,.003)
box("backpack reel orange bearing enclosure",(0,1.00,3.40),(.93,.28,.65),1,6,.025)
box("backpack reel recessed black field",(0,1.161,3.40),(.76,.045,.47),4,6,.009)
for x in (-.30,-.21,-.12,-.03,.06,.15,.24,.33):
    ring("backpack individually wound copper loop",(x,1.209,3.40),.185,.023,1,6,(0,1,0))
for x in (-.46,.46):
    box("backpack reel side retaining bracket",(x,1.20,3.40),(.11,.14,.58),2,6,.008)
box("backpack lower diagnostic bay",(0,.94,3.04),(.90,.24,.22),4,6,.014)
for x in (-.43,.43):
    bolt("diagnostic cabinet bolt",(x,1.157,4.46),(0,1,0),6,.029)
    bolt("lower reel bay bolt",(x,1.161,3.20),(0,1,0),6,.029)
for side in (-1,1):
    x=side*.63
    rod("high exposed backpack pipe",(x,.87,4.36),(x,.88,5.22),.045,2,6)
    cable("backpack shoulder bent pipe",[(x,.87,5.20),(x*.78,1.03,5.28),(x*.56,1.04,5.25)],.046,2,6)
    cable("backpack cable to arm",[(x,.78,3.58),(x*1.25,.77,3.28),(x*1.66,.50,2.91)],.055,4,6)
    cable("backpack orange arm feed",[(x*.73,1.19,3.77),
          (x*1.24,1.14,3.54),(x*1.37,.95,3.24),
          (x*1.56,.42,3.08)],.048,1,6)
    cyl("backpack upper service coupler",(x,1.03,4.80),.12,.30,2,6,(0,1,0),14)
    for z in (3.40,3.70,4.59):
        box("external pack structural latch",(x,.99,z),(.12,.20,.17),2,6,.009)
rod("rear upright communication antenna",(-.58,.82,4.68),(-.58,.82,5.49),.029,2,6)
cyl("antenna tip",(-.58,.82,5.49),.043,.06,2,6,(0,0,1),10)

# Sparse real geometry paint abrasions: especially the chest and thighs. These
# survive export, unlike the preview shader's procedural micro-scratching.
for pid, xcenter, y, zlo, zhi, spread in (
    (0,0,-.711,3.49,4.10,.34),
    (4,.50,-.452,1.75,2.33,.18),
    (5,-.50,-.452,1.75,2.33,.18),
):
    for i in range(12):
        x=xcenter+random.uniform(-spread,spread)
        z=random.uniform(zlo,zhi)
        box("exposed paint abrasion",(x,y,z),
            (random.uniform(.012,.035),.007,random.uniform(.010,.029)),2,pid,0)


def runtime(v):
    return (float(v.x),float(v.z),float(-v.y))


def game_position(v):
    return tuple(component * (1.8 / 5.35) for component in runtime(v))


def export_isemesh(path):
    bpy.context.view_layer.update()
    depsgraph=bpy.context.evaluated_depsgraph_get()
    groups=defaultdict(lambda:([],[]))
    maps={}
    triangles=0
    for obj in sorted(bpy.data.objects,key=lambda obj:obj.name):
        if obj.type not in ("MESH","CURVE") or "part_id" not in obj:
            continue
        pid=int(obj["part_id"])
        mid=int(obj["material_id"])
        evaluated=obj.evaluated_get(depsgraph)
        mesh=evaluated.to_mesh(preserve_all_data_layers=True,depsgraph=depsgraph)
        if mesh is None:
            continue
        mesh.calc_loop_triangles()
        triangles+=len(mesh.loop_triangles)
        world=evaluated.matrix_world
        normal_matrix=world.to_3x3().inverted().transposed()
        pivot=Vector(PARTS[pid][1])
        vertices,indices=groups[(pid,mid)]
        vertex_map=maps.setdefault((pid,mid),{})
        for triangle in mesh.loop_triangles:
            for loop_idx in triangle.loops:
                loop=mesh.loops[loop_idx]
                vert=mesh.vertices[loop.vertex_index]
                pos=world @ vert.co
                normal=(normal_matrix @ loop.normal).normalized()
                # Evaluated primitive UVs can change at rounding boundaries
                # between Blender runs. Stable world-space planar UVs also
                # keep a consistent texel density across separate metal parts.
                position=tuple(round(value, 4) for value in game_position(pos))
                direction=tuple(round(value, 4) for value in runtime(normal))
                dominant=max(range(3), key=lambda axis: abs(direction[axis]))
                if dominant == 0:
                    uv=(position[2],position[1])
                elif dominant == 1:
                    uv=(position[0],position[2])
                else:
                    uv=(position[0],position[1])
                record=(*position,*direction,*(round(value*2.0, 3) for value in uv))
                idx=vertex_map.get(record)
                if idx is None:
                    idx=len(vertices)
                    vertex_map[record]=idx
                    vertices.append(record)
                indices.append(idx)
        evaluated.to_mesh_clear()
    if len(groups)>32:
        raise RuntimeError(f"{len(groups)} material/part submeshes exceed 32 budget")
    with open(path,"wb") as out:
        out.write(b"ISEMESH1")
        out.write(struct.pack("<I",len(groups)))
        for (pid,mid),(vertices,indices) in sorted(groups.items()):
            out.write(struct.pack("<II3fII",pid,mid,*game_position(Vector(PARTS[pid][1])),len(vertices),len(indices)))
            for vertex in vertices:
                out.write(struct.pack("<8f",*vertex))
            for idx in indices:
                out.write(struct.pack("<I",idx))
    print(f"ENGINEER_EXPORT_SUBMESHES={len(groups)} TRIANGLES={triangles} VERTICES={sum(len(v[0]) for v in groups.values())} BYTES={path.stat().st_size}")


export_isemesh(EXPORT_DIR/"engineer_reference.isemesh")
if "--export-only" in sys.argv:
    raise SystemExit(0)

# Neutral review stage. Stage objects are never part of the runtime export.
floor=material("neutral review floor",(.29,.29,.29),0,.84)
bpy.ops.mesh.primitive_cube_add(size=1,location=(0,0,-.10))
stage=bpy.context.object
stage.name="review stage floor"
stage.dimensions=(40,40,.18)
bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
stage.data.materials.append(floor)
world=bpy.context.scene.world
world.use_nodes=True
world.node_tree.nodes["Background"].inputs["Color"].default_value=(.35,.35,.34,1)
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


area("neutral key",(-4,-5,8),950,5)
area("neutral fill",(4,-2,6),550,5)
area("back edge",(1,5,7),670,4)
camera_data=bpy.data.cameras.new("four-view camera")
camera=bpy.data.objects.new("four-view camera",camera_data)
bpy.context.collection.objects.link(camera)
bpy.context.scene.camera=camera
camera_data.type="ORTHO"
camera_data.ortho_scale=6.55
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
bpy.ops.wm.save_as_mainfile(filepath=str(MODEL_DIR/"engineer_reference.blend"))
print("ENGINEER_MODEL_PARTS="+",".join(PARTS[i][0] for i in range(8)))

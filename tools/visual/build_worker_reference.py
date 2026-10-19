"""Build a segmented Worker study from the four-view concept sheet.

Run from the repository root:
  blender -b --factory-startup --python tools/visual/build_worker_reference.py

The output is a review model. It does not change the in-game renderer.
"""

import math
import random
import struct
import sys
from collections import defaultdict
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
MODEL_DIR = ROOT / "assets/visual/models"
RENDER_DIR = ROOT / "visual/validation/worker_reference_blender"
MODEL_DIR.mkdir(parents=True, exist_ok=True)
RENDER_DIR.mkdir(parents=True, exist_ok=True)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
for material in list(bpy.data.materials):
    bpy.data.materials.remove(material)

random.seed(38017)
paint_image = bpy.data.images.load(str(MODEL_DIR / "worker_reference" / "worker_paint_albedo.png"))
paint_image.pack()


def material(name, color, metallic=0.0, roughness=0.5, emission=None):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    principled = mat.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = (*color, 1)
    principled.inputs["Metallic"].default_value = metallic
    principled.inputs["Roughness"].default_value = roughness
    if emission:
        principled.inputs["Emission Color"].default_value = (*emission[0], 1)
        principled.inputs["Emission Strength"].default_value = emission[1]
    return mat


def worn_orange(name, base):
    mat = material(name, base, 0.72, 0.46)
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    tex = nodes.new("ShaderNodeTexNoise")
    tex.inputs["Scale"].default_value = 26
    tex.inputs["Detail"].default_value = 4
    tex.inputs["Roughness"].default_value = 0.72
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.42
    ramp.color_ramp.elements[0].color = (*base, 1)
    ramp.color_ramp.elements[1].position = 0.62
    ramp.color_ramp.elements[1].color = (0.065, 0.072, 0.073, 1)
    ramp.color_ramp.elements.new(0.54).color = (base[0] * .62, base[1] * .50, base[2] * .40, 1)
    links.new(tex.outputs["Fac"], ramp.inputs["Fac"])
    paint = nodes.new("ShaderNodeTexImage")
    paint.image = paint_image
    paint.extension = "REPEAT"
    tint = nodes.new("ShaderNodeMixRGB")
    tint.blend_type = "MULTIPLY"
    tint.inputs[0].default_value = 1
    tint.inputs[2].default_value = (
        .76 if "dark" in name else .90 if "industrial" in name else 1.0,
        .72 if "dark" in name else .90 if "industrial" in name else 1.0,
        .70 if "dark" in name else .90 if "industrial" in name else 1.0,
        1,
    )
    links.new(paint.outputs["Color"], tint.inputs[1])
    links.new(tint.outputs["Color"], bsdf.inputs["Base Color"])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.09
    bump.inputs["Distance"].default_value = 0.018
    links.new(tex.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


orange = worn_orange("orange paint — industrial wear", (.59, .19, .025))
orange_light = worn_orange("orange paint — high panels", (.68, .26, .042))
orange_dark = worn_orange("orange paint — dark panels", (.40, .12, .014))
graphite = material("graphite joints", (.052, .059, .063), .66, .55)
steel = material("exposed gunmetal", (.16, .17, .17), .82, .37)
edge = material("cut dark steel", (.025, .028, .029), .74, .42)
silver = material("machined steel edges", (.32, .32, .30), .9, .32)
hazard = material("dark safety stripe", (.055, .052, .039), .25, .8)
amber = material("amber visor light", (.9, .24, .008), .1, .25, ((1.0, .24, .002), 2.4))
amber_dim = material("amber hardware", (.55, .23, .015), .4, .45, ((.8, .2, 0), .7))
rubber = material("hose rubber", (.016, .018, .018), .08, .77)


def root(name, pivot):
    obj = bpy.data.objects.new(name, None)
    bpy.context.collection.objects.link(obj)
    obj.empty_display_type = "ARROWS"
    obj.empty_display_size = .20
    obj.location = pivot
    obj["rigid_part"] = True
    return obj


parts = {
    "torso": root("torso", (0, 0, 3.55)),
    "head": root("head", (0, -.12, 4.65)),
    "right_arm": root("right_arm_saw_side", (-1.20, 0, 4.14)),
    "left_arm": root("left_arm_claw_side", (1.20, 0, 4.14)),
    "right_leg": root("right_leg", (-.62, 0, 2.55)),
    "left_leg": root("left_leg", (.62, 0, 2.55)),
    "back": root("backpack", (0, .52, 3.85)),
    "tool": root("tool_saw", (-1.49, -.06, 2.15)),
}


def parent_keep(obj, part):
    parent = parts[part] if isinstance(part, str) else part
    obj.parent = parent
    obj.matrix_parent_inverse = parent.matrix_world.inverted()
    return obj


def box(name, location, dimensions, mat, part, bevel=.025, rotation=None):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if rotation is not None:
        obj.rotation_euler = rotation
    obj.data.materials.append(mat)
    if mat in {orange, orange_light, orange_dark} and obj.data.uv_layers.active:
        offset_u, offset_v = random.random(), random.random()
        scale_u, scale_v = random.uniform(.35,.82), random.uniform(.35,.82)
        for uv_corner in obj.data.uv_layers.active.data:
            uv_corner.uv.x = uv_corner.uv.x*scale_u + offset_u
            uv_corner.uv.y = uv_corner.uv.y*scale_v + offset_v
    if bevel:
        modifier = obj.modifiers.new("small stamped bevel", "BEVEL")
        modifier.width = bevel
        modifier.segments = 2
        modifier.affect = "EDGES"
        obj.modifiers.new("weighted corner normals", "WEIGHTED_NORMAL")
    return parent_keep(obj, part)


def cylinder(name, location, radius, depth, mat, part, axis=(0, 0, 1), vertices=16):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler = Vector((0, 0, 1)).rotation_difference(Vector(axis)).to_euler()
    obj.data.materials.append(mat)
    bevel = obj.modifiers.new("machined rim", "BEVEL")
    bevel.width = min(.035, radius * .16)
    bevel.segments = 2
    obj.modifiers.new("weighted normals", "WEIGHTED_NORMAL")
    return parent_keep(obj, part)


def pipe(name, coords, radius, mat, part):
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.resolution_u = 12
    curve.bevel_depth = radius
    curve.bevel_resolution = 3
    spline = curve.splines.new("BEZIER")
    spline.bezier_points.add(len(coords) - 1)
    for point, pos in zip(spline.bezier_points, coords):
        point.co = pos
        point.handle_left_type = "AUTO"
        point.handle_right_type = "AUTO"
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    curve.materials.append(mat)
    return parent_keep(obj, part)


def bolt(name, pos, axis, part, radius=.045):
    cylinder(name + " recess", pos, radius * 1.45, .022, edge, part, axis, 12)
    raised = tuple(pos[i] + axis[i] * .016 for i in range(3))
    cylinder(name + " head", raised, radius, .026, steel, part, axis, 8)


def stripe(name, pos, dimensions, part, angle=-.45):
    # A thin strip on an XZ facing panel, slanted in the reference-sheet style.
    return box(name, pos, dimensions, hazard, part, 0, (0, angle, 0))


# Body core: the dark mechanism stays visible between independent armor pieces.
box("torso steel chassis", (0, .02, 3.56), (2.12, .96, 1.63), graphite, "torso", .09)
box("upper chest underplate", (0, -.51, 3.86), (2.02, .16, .93), edge, "torso", .06)
box("upper chest orange armor", (0, -.62, 3.88), (1.91, .13, .81), orange, "torso", .075)
for side in (-1, 1):
    box("overlapping pectoral side plate", (side*.65, -.713, 3.91), (.51, .09, .67), orange_light, "torso", .04)
    box("upper pectoral angled lip", (side*.63, -.757, 4.27), (.53, .13, .11), orange_dark, "torso", .024, (0,side*.10,0))
    box("torso flank exposed rib", (side*1.05, .00, 3.48), (.13, .93, .58), steel, "torso", .03)
    box("torso flank orange side plate", (side*1.13, .02, 3.85), (.15, .68, .75), orange, "torso", .032)
    box("side torso stepped lower armor", (side*1.00, -.04, 3.12), (.21, .53, .48), orange_dark, "torso", .035)
box("forward central chest apron", (0, -.73, 3.26), (1.05, .23, .77), orange_light, "torso", .045)
box("central apron lower steel edge", (0, -.858, 2.93), (.96, .07, .085), steel, "torso", .013)
box("lower abdomen graphite block", (0, -.46, 2.94), (1.23, .35, .40), graphite, "torso", .05)
box("left chest rail", (.93, -.65, 3.85), (.19, .20, 1.08), orange_dark, "torso", .035)
box("right chest rail", (-.93, -.65, 3.85), (.19, .20, 1.08), orange_dark, "torso", .035)
for x in (-.88, .88):
    for z in (3.38, 4.24):
        bolt("chest bolt", (x, -.75, z), (0, -1, 0), "torso")
for x in (-.30, -.10, .10, .30):
    stripe("chest caution marking", (x, -.861, 3.16), (.08, .012, .34), "torso")
for x in (-.72, .72):
    cylinder("waist hinge", (x, 0, 2.79), .23, .24, steel, "torso", (1, 0, 0))
box("pelvis central casting", (0, -.04, 2.57), (1.30, .76, .45), graphite, "torso", .06)
box("pelvis orange front shield", (0, -.45, 2.57), (.60, .11, .38), orange_dark, "torso", .04)

# The inset head is deliberately smaller than the chest.
box("neck socket", (0, -.10, 4.42), (.69, .62, .21), steel, "head", .045)
box("head underframe", (0, -.18, 4.77), (.80, .65, .65), graphite, "head", .055)
box("head orange helmet", (0, -.18, 4.91), (.74, .68, .64), orange_light, "head", .065)
box("head top inset armor", (0, -.18, 5.245), (.55, .52, .085), orange_dark, "head", .018)
box("visor dark frame", (0, -.545, 4.71), (.62, .11, .28), edge, "head", .028)
box("visor amber illuminated lens", (0, -.605, 4.73), (.43, .025, .17), amber, "head", .012)
box("visor top brow", (0, -.577, 4.89), (.64, .12, .095), steel, "head", .01)
for x in (-.29, .29):
    bolt("head temple fastener", (x, -.57, 5.03), (0, -1, 0), "head", .034)
    cylinder("head side rotary hinge", (x * 1.35, -.15, 4.74), .20, .14, steel, "head", (1, 0, 0))
pipe("left over-head conduit", [(-.36, .02, 4.85), (-.44, .11, 5.29), (-.20, .24, 5.41)], .042, rubber, "head")
pipe("right over-head conduit", [(.36, .02, 4.85), (.44, .11, 5.29), (.20, .24, 5.41)], .042, rubber, "head")


def leg(x, part):
    cylinder("hip full-width bearing", (x, 0, 2.47), .33, .34, steel, part, (1, 0, 0), 20)
    cylinder("hip dark center", (x + math.copysign(.19, x), 0, 2.47), .19, .035, edge, part, (1, 0, 0), 16)
    box("upper leg hydraulics", (x, .01, 1.99), (.50, .53, .72), steel, part, .045)
    box("upper leg orange plate", (x, -.34, 2.02), (.56, .15, .65), orange, part, .045)
    box("rear thigh maintenance armor", (x, .34, 2.04), (.52, .14, .66), orange, part, .04)
    box("thigh raised forward armor", (x, -.445, 2.06), (.44, .08, .43), orange_light, part, .025)
    box("thigh lower dark steel lip", (x, -.42, 1.72), (.51, .11, .075), steel, part, .013)
    box("upper leg side armor", (x + math.copysign(.30, x), .01, 2.00), (.15, .65, .70), orange_light, part, .03)
    cylinder("knee exposed hinge", (x, .02, 1.53), .24, .73, graphite, part, (1, 0, 0), 18)
    cylinder("knee ring", (x + math.copysign(.38, x), .02, 1.53), .18, .065, silver, part, (1, 0, 0), 18)
    cylinder("front knee bearing", (x, -.36, 1.52), .17, .10, steel, part, (0, -1, 0), 18)
    cylinder("front knee dark hub", (x, -.419, 1.52), .09, .035, edge, part, (0, -1, 0), 14)
    box("shin frame", (x, 0, .99), (.54, .58, .79), graphite, part, .04)
    box("shin orange plate", (x, -.36, 1.02), (.62, .18, .80), orange, part, .045)
    box("rear shin maintenance armor", (x, .34, 1.01), (.54, .15, .76), orange, part, .036)
    box("shin forward overlapping shield", (x, -.475, 1.07), (.48, .09, .56), orange_light, part, .028)
    box("shin lower exposed metal rim", (x, -.478, .71), (.52, .09, .08), steel, part, .012)
    box("side shin armor", (x + math.copysign(.31, x), .02, .97), (.11, .60, .73), orange_dark, part, .035)
    cylinder("ankle rotor", (x, .03, .51), .21, .72, steel, part, (1, 0, 0), 16)
    box("massive foot frame", (x, -.26, .23), (.70, 1.10, .39), graphite, part, .055)
    box("front toe cap", (x, -.66, .21), (.75, .37, .29), steel, part, .035)
    box("foot orange armor", (x, -.28, .39), (.65, .77, .19), orange_light, part, .03)
    box("foot raised instep armor", (x, -.40, .51), (.56, .47, .10), orange, part, .02)
    box("rear heel painted cap", (x, .31, .29), (.62, .25, .28), orange_dark, part, .028)
    for t in (-.23, 0, .23):
        box("segmented toe", (x+t, -.91, .18), (.20, .20, .17), steel, part, .02)
    for z in (.77, 1.24, 2.27):
        bolt("leg rivet", (x + math.copysign(.21, x), -.48, z), (0, -1, 0), part, .033)
    pipe("rear hydraulic hose", [(x, .28, 2.35), (x, .40, 1.92), (x, .34, 1.55), (x, .30, .90)], .039, rubber, part)
    outer_x = x + math.copysign(.40, x)
    cylinder("outer hip exposed bearing", (outer_x, .06, 2.43), .22, .12, steel, part, (1, 0, 0), 18)
    cylinder("outer hip center axle", (outer_x + math.copysign(.07,x), .06, 2.43), .11, .035, edge, part, (1,0,0), 16)
    cylinder("outer knee machined bearing", (outer_x, .04, 1.50), .22, .12, silver, part, (1,0,0), 18)
    pipe("outer leg hydraulic piston tube", [(outer_x, .27, 2.26), (outer_x, .33, 1.99), (outer_x, .27, 1.69)], .075, graphite, part)
    pipe("outer leg polished piston rod", [(outer_x, .30, 1.98), (outer_x, .27, 1.68)], .035, silver, part)
    cylinder("rear knee exposed bearing", (x, .47, 1.52), .19, .13, steel, part, (0,1,0), 18)
    cylinder("rear knee center cap", (x, .545, 1.52), .085, .03, edge, part, (0,1,0), 14)
    cylinder("rear ankle roller bearing", (x, .47, .53), .17, .14, silver, part, (0,1,0), 18)
    pipe("rear lower leg actuator", [(x+math.copysign(.22,x), .50, 1.37), (x+math.copysign(.21,x), .58, 1.05), (x+math.copysign(.18,x), .51, .66)], .052, steel, part)


leg(-.64, "right_leg")
leg(.64, "left_leg")


def arm(x, part, claw=False):
    side = math.copysign(1, x)
    cylinder("shoulder mechanical socket", (x, 0, 4.13), .30, .39, steel, part, (1, 0, 0), 20)
    cylinder("shoulder graphite pivot", (x + side * .22, 0, 4.13), .22, .11, graphite, part, (1, 0, 0), 18)
    box("outer shoulder armor", (x + side * .22, -.03, 4.04), (.36, .81, .87), orange_light, part, .065, (0, side * -.08, 0))
    box("shoulder front overlapping plate", (x+side*.20, -.487, 4.10), (.39, .13, .57), orange, part, .035)
    box("shoulder lower metal hem", (x+side*.20, -.50, 3.76), (.42, .11, .075), steel, part, .012)
    box("upper arm exposed mechanism", (x + side*.10, .02, 3.47), (.37, .48, .72), steel, part, .045)
    box("upper arm front painted strip", (x + side*.10, -.27, 3.56), (.38, .14, .60), orange, part, .035)
    box("upper arm rear painted plate", (x + side*.10, .28, 3.54), (.38, .13, .62), orange, part, .034)
    cylinder("elbow black rotary joint", (x + side*.09, .01, 3.04), .22, .45, graphite, part, (1, 0, 0), 18)
    cylinder("elbow circular steel face", (x + side*.31, .01, 3.04), .15, .035, steel, part, (1, 0, 0), 16)
    box("forearm mechanical chassis", (x + side*.10, -.02, 2.59), (.40, .56, .73), steel, part, .045)
    box("forearm orange armor", (x + side*.12, -.32, 2.61), (.45, .16, .68), orange, part, .04)
    box("forearm rear painted armor", (x + side*.12, .31, 2.61), (.42, .15, .66), orange, part, .038)
    box("forearm upper overlapping panel", (x+side*.12, -.424, 2.75), (.38, .075, .30), orange_light, part, .02)
    box("forearm lower wrist rim", (x+side*.12, -.424, 2.30), (.39, .06, .065), steel, part, .01)
    for z in (2.42, 2.83, 3.84, 4.20):
        bolt("arm plate rivet", (x + side*.08, -.44, z), (0, -1, 0), part, .036)
    pipe("outside arm cable", [(x + side*.27, .20, 4.16), (x + side*.34, .33, 3.55), (x + side*.30, .22, 3.02), (x + side*.29, .16, 2.55)], .042, rubber, part)
    if claw:
        cylinder("wrist bearing", (x + side*.10, -.02, 2.17), .18, .31, steel, part)
        box("claw palm", (x + side*.1, -.04, 1.98), (.44, .40, .29), graphite, part, .035)
        for dx in (-.16, 0, .16):
            box("articulated claw finger", (x+side*.1+dx, -.21, 1.71), (.12, .18, .46), steel, part, .023, (-.18, 0, dx*.16))
            box("claw fingertip", (x+side*.1+dx, -.30, 1.49), (.115, .17, .16), graphite, part, .012)
        box("claw thumb", (x - side*.23, -.15, 1.84), (.15, .22, .36), steel, part, .025, (0, side*.22, 0))


arm(-1.29, "right_arm")
arm(1.29, "left_arm", claw=True)

# Rear maintenance pack and mirrored pipe runs. Back has its own animation pivot.
box("rear equipment rack", (0, .55, 3.67), (1.54, .37, 1.67), graphite, "back", .055)
box("rear projecting spine casting", (0, .83, 3.28), (.62, .48, .71), steel, "back", .04)
box("rear lower service armor", (0, 1.09, 3.18), (.53, .09, .42), orange_dark, "back", .025)
for x in (-.79, .79):
    box("rear orange vertical rail", (x, .84, 3.78), (.25, .22, 1.52), orange, "back", .04)
    for z in (3.21, 4.28):
        bolt("rear rail bolt", (x, .967, z), (0, 1, 0), "back")
box("rear maintenance orange hatch", (0, .83, 3.77), (1.00, .16, 1.22), orange_light, "back", .04)
box("rear vent black recess", (0, .936, 3.71), (.72, .045, .42), edge, "back", .012)
for z in (3.56, 3.64, 3.72, 3.80, 3.88):
    box("vent grille louver", (0, .972, z), (.66, .05, .035), silver, "back", .007)
for x in (-.55, .55):
    pipe("rear hose upper", [(x, .42, 4.46), (x, .63, 4.70), (x*.88, .75, 4.81), (x*.74, .82, 4.60)], .075, rubber, "back")
    pipe("rear hose lower", [(x*.65, 1.07, 3.34), (x*.88, 1.14, 3.05), (x, .68, 2.74)], .085, rubber, "back")
    pipe("rear secondary armored tube", [(x*1.34, .93, 4.41), (x*1.36, 1.04, 3.85), (x*.95, 1.07, 3.36)], .045, steel, "back")
    cylinder("rear mount", (x, .80, 4.50), .13, .20, steel, "back", (0, 1, 0))
    cylinder("rear hip machinery bearing", (x*1.15, .49, 2.65), .21, .23, steel, "back", (0,1,0), 20)
    cylinder("rear hip center axle", (x*1.15, .63, 2.65), .10, .035, edge, "back", (0,1,0), 16)
pipe("rear horizontal cooling pipe", [(-.65, .97, 4.37), (-.42, 1.05, 4.49), (.42, 1.05, 4.49), (.65, .97, 4.37)], .055, steel, "back")


# Large circular saw blade. The blade lies in YZ, so it is edge-on from the front,
# but its full toothed profile reads in the left view, matching the sheet.
SAW_X, SAW_Y, SAW_Z = -1.55, -.08, 1.77
cylinder("saw dark blade core", (SAW_X, SAW_Y, SAW_Z), .70, .16, edge, "tool", (1, 0, 0), 32)
cylinder("saw steel blade visible face", (SAW_X-.093, SAW_Y, SAW_Z), .65, .045, steel, "tool", (1, 0, 0), 48)
cylinder("saw toothed perimeter", (SAW_X, SAW_Y, SAW_Z), .78, .12, silver, "tool", (1, 0, 0), 48)
cylinder("saw inner dark concentric disk", (SAW_X-.16, SAW_Y, SAW_Z), .59, .026, steel, "tool", (1, 0, 0), 40)

def saw_tooth(index, count=28):
    angle = 2 * math.pi * index / count
    # a tangentially slanted tooth visibly breaks the outer silhouette
    corners = []
    for radius, step in ((.75, -.36), (.91, -.18), (.91, .21), (.75, .43)):
        a = angle + step * (2*math.pi/count)
        corners.append((SAW_Y + radius*math.cos(a), SAW_Z + radius*math.sin(a)))
    verts = [(xx, yy, zz) for xx in (SAW_X-.105, SAW_X+.105) for yy,zz in corners]
    faces = [(0, 1, 2, 3), (4, 7, 6, 5)]
    faces += [(a, (a+1)%4, (a+1)%4+4, a+4) for a in range(4)]
    mesh = bpy.data.meshes.new("machined saw tooth")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(f"saw cutting tooth {index:02d}", mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(silver if index%4 else steel)
    parent_keep(obj, "tool")

for i in range(28):
    saw_tooth(i)

for radius, thickness, mat in ((.53, .035, edge), (.38, .055, orange_dark), (.24, .07, graphite), (.13, .08, silver)):
    cylinder("saw radial hub", (SAW_X-.20-thickness*.2, SAW_Y, SAW_Z), radius, thickness, mat, "tool", (1, 0, 0), 36)
for i in range(8):
    a = 2*math.pi*i/8
    cylinder("saw hub fastener", (SAW_X-.267, SAW_Y+.32*math.cos(a), SAW_Z+.32*math.sin(a)), .034, .025, steel, "tool", (1,0,0), 8)

# An upper half guard leaves the bottom teeth uncovered. Its front orange
# housing links the blade to the forearm, instead of a freestanding wheel.
box("saw forearm drive housing", (-1.54, -.21, 2.48), (.60, .72, .77), orange_light, "tool", .06)
box("saw safety guard bracket", (-1.63, -.18, 2.30), (.31, .50, .43), orange_dark, "tool", .035)
for i in range(15):
    a = math.radians(5 + i*12)
    cy = SAW_Y + .79*math.cos(a)
    cz = SAW_Z + .79*math.sin(a)
    box("upper saw orange guard segment", (SAW_X-.19, cy, cz), (.12, .22, .13), orange if i%3 else orange_light, "tool", .012, (a, 0, 0))

def saw_guard_sector():
    angles = [math.radians(20 + i*140/24) for i in range(25)]
    verts = []
    for x in (SAW_X-.31, SAW_X-.22):
        for radius in (.61, .81):
            verts.extend((x, SAW_Y+radius*math.cos(a), SAW_Z+radius*math.sin(a)) for a in angles)
    faces = []
    count = len(angles)
    for i in range(count-1):
        for ring in (0, 2):
            base = ring*count+i
            faces.append((base, base+1, base+1+count, base+count))
        faces.append((i, i+1, i+1+2*count, i+2*count))
        faces.append((i+count, i+1+count, i+1+3*count, i+3*count))
    mesh = bpy.data.meshes.new("formed saw guard metal")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new("integrated orange upper saw guard", mesh)
    bpy.context.collection.objects.link(obj)
    mesh.materials.append(orange_dark)
    parent_keep(obj, "tool")

saw_guard_sector()
box("saw side drive connection", (-1.81, -.07, 2.32), (.11, .48, .59), orange, "tool", .035)
box("saw angled side guard", (-1.83, -.34, 2.53), (.08, .44, .36), orange_light, "tool", .024, (.23,0,0))
for i in range(3):
    stripe("saw caution marking", (-1.86, -.28+i*.18, 2.49), (.012, .085, .34), "tool", -.43)
pipe("saw drive hydraulic line", [(-1.34, .27, 2.99), (-1.52, .41, 2.65), (-1.61, .48, 2.24), (-1.60, .22, 1.89)], .047, rubber, "tool")

# Selectively add fixed chips on armor to supplement procedural surface wear.
for side in (-1, 1):
    for idx in range(11):
        x = side*.64 + random.uniform(-.22, .22)
        z = random.uniform(.72, 1.29)
        box("irregular shin paint chip", (x, -.472, z), (random.uniform(.015,.05), .011, random.uniform(.01,.045)), steel, "right_leg" if side<0 else "left_leg", 0)
for idx in range(28):
    x = random.uniform(-.79, .79)
    z = random.uniform(3.49, 4.22)
    box("irregular chest paint chip", (x, -.695, z), (random.uniform(.012,.05), .012, random.uniform(.008,.035)), steel, "torso", 0)
for side in (-1, 1):
    for idx in range(24):
        x = side*random.uniform(.75, .96)
        z = random.uniform(3.58, 4.26)
        box("chest edge scraped steel", (x, -.775, z), (random.uniform(.025,.11), .01, random.uniform(.014,.06)), steel if idx%3 else graphite, "torso", 0, (0, random.uniform(-.5,.5), 0))
    for idx in range(22):
        x = side*.64+random.uniform(-.29,.29)
        z = random.uniform(.74, 1.38)
        box("shin chipped orange edge", (x, -.53, z), (random.uniform(.02,.09), .01, random.uniform(.014,.06)), steel if idx%4 else edge, "right_leg" if side<0 else "left_leg", 0, (0,random.uniform(-.5,.5),0))
    for idx in range(17):
        x = side*.64+random.uniform(-.27,.27)
        z = random.uniform(1.76, 2.37)
        box("thigh paint scrape", (x, -.49, z), (random.uniform(.02,.08), .01, random.uniform(.01,.055)), steel, "right_leg" if side<0 else "left_leg", 0, (0,random.uniform(-.4,.4),0))

# Logical hierarchy. Pivot positions remain independent for later rigging.
parent_keep(parts["head"], "torso")
parent_keep(parts["back"], "torso")
parent_keep(parts["tool"], "right_arm")

# Studio render with strictly orthographic front/side/back/side views.
studio_floor = material("neutral studio floor", (.29, .29, .28), .04, .79)
box("studio floor", (0, 0, -.12), (200, 200, .20), studio_floor, parts["torso"], 0)
# The floor is scene furniture; remove the torso parenting that box() provides.
floor = bpy.data.objects.get("studio floor")
floor.parent = None

world = bpy.context.scene.world
world.color = (.40, .40, .40)
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (.35, .35, .34, 1)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = .65

def area(name, pos, power, size, target=(0,0,2.9)):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = power
    data.shape = "DISK"
    data.size = size
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    obj.location = pos
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()

area("neutral soft key", (-4,-5,8), 620, 5)
area("neutral soft fill", (4,-2,5), 340, 5)
area("rear rim", (1,4,7), 500, 3)
visor_bounce_data = bpy.data.lights.new("visor amber bounce", "POINT")
visor_bounce_data.energy = 18
visor_bounce_data.color = (1.0, .27, .012)
visor_bounce_data.shadow_soft_size = .22
visor_bounce = bpy.data.objects.new("visor amber bounce", visor_bounce_data)
bpy.context.collection.objects.link(visor_bounce)
visor_bounce.location = (0, -.66, 4.76)

camera_data = bpy.data.cameras.new("four-view orthographic camera")
camera = bpy.data.objects.new("four-view orthographic camera", camera_data)
bpy.context.collection.objects.link(camera)
bpy.context.scene.camera = camera
camera_data.type = "ORTHO"
camera_data.ortho_scale = 6.6

scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = 680
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.film_transparent = False
scene.render.filepath = str(RENDER_DIR / "front.png")
scene.view_settings.view_transform = "Standard"

views = {
    "front": (0,-13,3.0),
    "left_side": (-13,0,3.0),
    "back": (0,13,3.0),
    "right_side": (13,0,3.0),
}
if "--export-only" not in sys.argv:
    for name, position in views.items():
        camera.location = position
        camera.rotation_euler = (Vector((0,0,2.65)) - camera.location).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = str(RENDER_DIR / f"{name}.png")
        bpy.ops.render.render(write_still=True)

camera.location = views["front"]
camera.rotation_euler = (Vector((0,0,2.65)) - camera.location).to_track_quat("-Z", "Y").to_euler()
scene.render.filepath = str(RENDER_DIR / "front.png")
bpy.ops.wm.save_as_mainfile(filepath=str(MODEL_DIR / "worker_reference.blend"))

mesh_objects = [obj for obj in bpy.data.objects if obj.type in {"MESH", "CURVE"} and obj != floor]
poly_count = sum(len(obj.data.polygons) for obj in mesh_objects if obj.type == "MESH")
print(f"WORKER_REFERENCE_MESHES={len(mesh_objects)} POLYGONS={poly_count}")
print("WORKER_REFERENCE_PARTS=" + ",".join(parts))

# Compact engine interchange: draw groups are (rigid part, material), while the
# editable .blend keeps every screw, panel and finger separate. All vertices
# are assembled world-space coordinates, scaled to a 1.8 m tall game character.
part_ids = {
    "torso": 0, "head": 1, "left_arm": 2, "right_arm": 3,
    "left_leg": 4, "right_leg": 5, "back": 6, "tool": 7,
}
material_ids = {
    orange: 0, orange_light: 0, orange_dark: 0,
    graphite: 1, edge: 1,
    steel: 2, silver: 2,
    amber: 3, amber_dim: 3,
    # Rubber and graphite share the dark in-game draw group; Blender retains
    # separate roughness/material controls in the editable source.
    rubber: 1,
    hazard: 5,
}
scale = 1.8 / 5.35

def game_coord(v):
    return (v.x * scale, v.z * scale, -v.y * scale)

def rigid_part_for(obj):
    current = obj.parent
    while current is not None:
        if current.get("rigid_part"):
            return current
        current = current.parent
    raise RuntimeError(f"mesh without rigid parent: {obj.name}")

root_ids = {parts[name].name: part_ids[name] for name in parts}
groups = defaultdict(lambda: {"vertices": [], "indices": [], "dedup": {}})
# The primary export retains every bevel used in the four review renders.
# The optional LOD only runs when explicitly requested for measured testing.
low_poly_export = "--low-poly-export" in sys.argv
if low_poly_export:
    for obj in mesh_objects:
        for modifier in obj.modifiers:
            if modifier.type in {"BEVEL", "WEIGHTED_NORMAL"}:
                modifier.show_viewport = False
depsgraph = bpy.context.evaluated_depsgraph_get()
depsgraph.update()
for obj in mesh_objects:
    part = rigid_part_for(obj)
    part_id = root_ids[part.name]
    # Part 8 is the spinning saw disk. It follows tool part 7's attack pose,
    # but the renderer also rotates it around its own axle.
    if part_id == 7 and obj.name.startswith((
            "saw dark blade core", "saw steel blade visible face",
            "saw toothed perimeter", "saw inner dark concentric disk",
            "saw cutting tooth", "saw radial hub", "saw hub fastener")):
        part_id = 8
    material_id = material_ids[obj.data.materials[0]]
    group = groups[(part_id, material_id)]
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    mesh.calc_loop_triangles()
    normal_matrix = evaluated.matrix_world.to_3x3().inverted().transposed()
    for tri in mesh.loop_triangles:
        for loop_index in tri.loops:
            local_vertex = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            position = game_coord(evaluated.matrix_world @ local_vertex)
            local_normal = mesh.corner_normals[loop_index].vector
            n = (normal_matrix @ local_normal).normalized()
            normal = (n.x, n.z, -n.y)
            if mesh.uv_layers.active is not None:
                texcoord = mesh.uv_layers.active.data[loop_index].uv
                uv = (texcoord.x, texcoord.y)
            elif abs(n.y) >= abs(n.x) and abs(n.y) >= abs(n.z):
                uv = (position[0] * .8, position[1] * .8)
            elif abs(n.x) >= abs(n.z):
                uv = (position[2] * .8, position[1] * .8)
            else:
                uv = (position[0] * .8, position[2] * .8)
            vertex = (*position, *normal, *uv)
            key = tuple(round(value, 5) for value in vertex)
            index = group["dedup"].get(key)
            if index is None:
                index = len(group["vertices"])
                group["dedup"][key] = index
                group["vertices"].append(vertex)
            group["indices"].append(index)
    evaluated.to_mesh_clear()

binary_name = "worker_reference_lod1.isemesh" if low_poly_export else "worker_reference.isemesh"
binary_path = MODEL_DIR / "worker_reference" / binary_name
binary_path.parent.mkdir(parents=True, exist_ok=True)
with binary_path.open("wb") as output:
    output.write(b"ISEMESH1")
    output.write(struct.pack("<I", len(groups)))
    for (part_id, material_id), group in sorted(groups.items()):
        name = next(name for name, value in part_ids.items() if value == min(part_id, 7))
        pivot = game_coord(parts[name].matrix_world.translation)
        if part_id == 8:
            pivot = game_coord(Vector((SAW_X, SAW_Y, SAW_Z)))
        vertices = group["vertices"]
        indices = group["indices"]
        output.write(struct.pack("<II3fII", part_id, material_id, *pivot, len(vertices), len(indices)))
        for vertex in vertices:
            output.write(struct.pack("<8f", *vertex))
        output.write(struct.pack(f"<{len(indices)}I", *indices))
print(f"WORKER_REFERENCE_SUBMESHES={len(groups)} VERTICES={sum(len(g['vertices']) for g in groups.values())} TRIANGLES={sum(len(g['indices']) for g in groups.values())//3}")
print(f"WORKER_REFERENCE_BINARY={binary_path}")

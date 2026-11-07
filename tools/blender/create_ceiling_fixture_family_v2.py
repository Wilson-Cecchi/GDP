"""Create editable ceiling-fixture concepts and review renders (Blender 4.3+).

Run from repository root:
  blender -b -t 2 --python-exit-code 1 --python tools/blender/create_ceiling_fixture_family_v2.py

Only writes visual/proposals/ceiling_fixture_family_v2/. Nothing is exported
to game runtime assets. Units are meters; the luminous face points down (-Z).
"""

import math
import os
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "visual/proposals/ceiling_fixture_family_v2"
OUT.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0

for obj in list(bpy.data.objects):
    bpy.data.objects.remove(obj, do_unlink=True)
for scene in list(bpy.data.scenes)[1:]:
    bpy.data.scenes.remove(scene, do_unlink=True)

MATS = {}


def material(name, rgb, metallic=0.0, roughness=0.7, emission=0.0):
    key = (name, tuple(rgb), metallic, roughness, emission)
    if key in MATS:
        return MATS[key]
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*rgb, 1)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Emission Color"].default_value = (*rgb, 1)
    bsdf.inputs["Emission Strength"].default_value = emission
    MATS[key] = mat
    return mat


BODY = {
    "Industrial": (0.105, 0.119, 0.125),
    "Storage": (0.18, 0.20, 0.19),
    "Maintenance": (0.15, 0.17, 0.175),
    "Laboratory": (0.49, 0.54, 0.55),
    "Cafeteria": (0.47, 0.45, 0.40),
    "Security": (0.13, 0.16, 0.17),
    "Office": (0.39, 0.43, 0.44),
    "Boss / Arena": (0.125, 0.125, 0.13),
    "Central Hub": (0.155, 0.20, 0.22),
}


def lerp(a, b, t):
    return tuple((1-t)*x+t*y for x, y in zip(a, b))


def cube(scene, name, location, dimensions, mat, bevel=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    if bevel:
        mod = obj.modifiers.new("Soft machined edge", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        obj.modifiers.new("Weighted face normals", "WEIGHTED_NORMAL")
    obj["fixture_part"] = True
    return obj


def bolt(scene, name, x, y, z, mat, radius=0.012):
    bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=radius,
                                        depth=0.007, location=(x, y, z))
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(mat)
    obj["fixture_part"] = True
    return obj


def add_fixture(scene, theme, center=(0, 0, 0), condition="Used", state="ON"):
    x, y, z = center
    slug = theme.replace(" ", "_").replace("/", "_")
    prefix = f"{slug}_{condition}_{state}"
    color = BODY[theme]
    # A condition changes surface finish. It does not switch the light off.
    if condition == "Maintained":
        paint_color, roughness, edge = color, 0.42, 0.12
    elif condition == "Worn":
        paint_color, roughness, edge = lerp(color, (0.35, 0.24, 0.14), 0.60), 0.86, 0.50
    else:
        paint_color, roughness, edge = lerp(color, (0.16, 0.16, 0.16), 0.12), 0.57, 0.24
    body = material(f"{prefix}/paint", paint_color, 0.48, roughness)
    edge_target = ((0.40, 0.25, 0.14) if condition == "Worn"
                   else (0.58, 0.60, 0.57))
    bare = material(f"{prefix}/edge metal", lerp(paint_color, edge_target, edge), 0.79, 0.36)
    seal = material("Fixture/common black gasket", (0.022, 0.028, 0.031), 0.05, 0.92)
    screw = material("Fixture/common fasteners", (0.27, 0.30, 0.31), 0.8, 0.35)
    if state == "OFF":
        lens_color, strength = (0.48, 0.52, 0.52), 0.0
    elif state == "ALERT":
        lens_color, strength = (0.95, 0.13, 0.09), 1.0
    else:
        lens_color, strength = ((0.96, 0.91, 0.80) if theme == "Cafeteria"
                                else (0.78, 0.88, 0.91)), 0.7
    lens = material(f"{prefix}/separate emissive lens", lens_color, 0.0, 0.64, strength)

    thickness = 0.09 if theme == "Office" else 0.12
    width = 0.30 if theme in ("Laboratory", "Cafeteria", "Office") else 0.28
    housing = cube(scene, f"{prefix}/Housing", (x,y,z),
                   (1.20,width,thickness), body, 0.015)
    housing["fixture_housing"] = True
    housing["room_variant"] = theme
    housing["material_condition"] = condition
    housing["emission_state"] = state
    cube(scene, f"{prefix}/Gasket", (x,y,z-thickness/2-0.004), (1.13,width-0.028,0.009), seal, 0.007)
    cube(scene, f"{prefix}/Diffuser lens", (x,y,z-thickness/2-0.011),
         (1.04,width-0.085,0.011), lens, 0.008)
    # Housing edge and replaceable end caps establish a consistent family.
    for sy in (-1,1):
        cube(scene, f"{prefix}/Side rim {sy}", (x,y+sy*(width/2-0.018),z-thickness/2-0.014),
             (1.14,0.020,0.017), body, 0.004)
    for sx in (-1,1):
        cube(scene, f"{prefix}/End cap {sx}", (x+sx*0.563,y,z-thickness/2-0.015),
             (0.031,width-0.012,0.024), bare, 0.004)
        for sy in (-1,1):
            bolt(scene, f"{prefix}/Screw {sx} {sy}", x+sx*0.548,
                 y+sy*(width/2-0.047), z-thickness/2-0.031, screw)

    # Deterministic paint wear and dusty diffuser scratches, distinct from
    # emission state. No random hash, so the .blend is reproducible.
    chip_count = {"Maintained": 0, "Used": 3, "Worn": 10}[condition]
    grime = material("Fixture/diffuser dust", (0.28,0.30,0.29),0.0,0.95)
    for n in range(chip_count):
        sx = -0.45 + ((n*37)%97)/97*0.90
        sy = -1 if n%2 else 1
        cube(scene,f"{prefix}/Worn paint chip {n}",
             (x+sx,y+sy*(width/2-0.027),z-thickness/2-0.025),
             (0.028+0.008*(n%3),0.014,0.002),bare)
        if n%2 == 0:
            cube(scene,f"{prefix}/Diffuser dust mark {n}",
                 (x+sx,y+sy*(width/2-0.054),z-thickness/2-0.019),
                 (0.017,0.004,0.001),grime)

    if theme == "Industrial":
        for sx in (-0.36,0.36):
            cube(scene, f"{prefix}/Body stiffener", (x+sx,y,z-thickness/2-0.022),
                 (0.022,width-0.025,0.026), bare, 0.003)
    elif theme == "Storage":
        # Narrow safety bars retain the readable long-line silhouette.
        for sx in (-0.39,-0.13,0.13,0.39):
            cube(scene, f"{prefix}/Cage crossbar", (x+sx,y,z-thickness/2-0.029),
                 (0.012,width-0.045,0.014), bare, 0.003)
        for sy in (-0.084,0.084):
            cube(scene, f"{prefix}/Cage rail", (x,y+sy,z-thickness/2-0.029),
                 (1.10,0.009,0.014), bare, 0.003)
    elif theme == "Maintenance":
        for sx in (-0.43,0.43):
            cube(scene, f"{prefix}/Service latch", (x+sx,y,z-thickness/2-0.027),
                 (0.075,0.036,0.022), bare, 0.005)
        cube(scene, f"{prefix}/Narrow service spine", (x,y,z-thickness/2-0.027),
             (0.78,0.018,0.018), body, 0.003)
    elif theme == "Laboratory":
        for sy in (-1,1):
            cube(scene, f"{prefix}/Continuous seal {sy}", (x,y+sy*0.12,z-thickness/2-0.022),
                 (1.08,0.010,0.011), seal, 0.003)
    elif theme == "Cafeteria":
        # Side skirt softens the housing without changing the ceiling module.
        for sy in (-1,1):
            cube(scene, f"{prefix}/Soft side skirt {sy}", (x,y+sy*0.135,z-thickness/2-0.015),
                 (1.04,0.025,0.017), body, 0.008)
    elif theme == "Security":
        for sx in (-0.48,0.48):
            cube(scene, f"{prefix}/Reinforced yoke {sx}", (x+sx,y,z-thickness/2-0.022),
                 (0.042,width-0.022,0.033), bare, 0.004)
        bolt(scene, f"{prefix}/Unlit sensor", x+0.49,y,z-thickness/2-0.043,seal,0.023)
        # The two fixed rods hang this fixture below the ceiling in the
        # Security T layout. In standard Security rooms the rods are hidden
        # inside the ceiling, retaining the existing flush-mounted reading.
        for sx in (-0.38, 0.38):
            bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=0.013,
                                                depth=0.28,
                                                location=(x+sx, y, z+0.19))
            rod = bpy.context.object
            rod.name = f"{prefix}/Suspension rod {sx}"
            rod.data.materials.append(bare)
            rod["fixture_part"] = True
            cube(scene, f"{prefix}/Ceiling mount {sx}",
                 (x+sx,y,z+0.335), (0.16,0.11,0.03), body, 0.004)
    elif theme == "Office":
        for sx in (-1,1):
            cube(scene, f"{prefix}/Thin corner clasp {sx}",
                 (x+sx*0.52,y,z-thickness/2-0.018), (0.023,width-0.055,0.015),bare,0.004)
    elif theme == "Boss / Arena":
        for sx in (-0.43,-0.22,0.22,0.43):
            cube(scene, f"{prefix}/Impact bar", (x+sx,y,z-thickness/2-0.034),
                 (0.026,width-0.025,0.025),bare,0.004)
        for sy in (-1,1):
            cube(scene, f"{prefix}/Deep guard rail {sy}",
                 (x,y+sy*0.105,z-thickness/2-0.035),(1.1,0.025,0.029),body,0.004)
    elif theme == "Central Hub":
        cube(scene, f"{prefix}/Axial separator", (x,y,z-thickness/2-0.026),
             (1.09,0.018,0.018),body,0.004)
        for sx in (-0.42,0.42):
            cube(scene, f"{prefix}/Axis marker", (x+sx,y,z-thickness/2-0.031),
                 (0.025,0.025,0.020),bare,0.003)
    return prefix


def new_scene(name, resolution, ortho_scale, camera_xyz=(0,0,-12), target=(0,0,0)):
    if name == "Family board":
        scene = bpy.data.scenes[0]
        scene.name = name
    else:
        scene = bpy.data.scenes.new(name)
    bpy.context.window.scene = scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x, scene.render.resolution_y = resolution
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.render.filepath = str(OUT / f"{name.lower().replace(' ', '_')}.png")
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.image_settings.color_mode = "RGBA"
    scene.world = bpy.data.worlds.new(f"{name}/studio world")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.055,0.07,0.08,1)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.35
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"
    cam = bpy.data.cameras.new(f"{name}/camera")
    cam.type = "ORTHO"
    cam.ortho_scale = ortho_scale
    camera = bpy.data.objects.new(cam.name, cam)
    scene.collection.objects.link(camera)
    camera.location = camera_xyz
    camera.rotation_euler = (Vector(target)-camera.location).to_track_quat('-Z','Y').to_euler()
    if camera_xyz == (0,0,-12):
        # The underside view otherwise reads right-to-left and upside down.
        camera.rotation_euler.rotate_axis('Z', math.pi)
    scene.camera = camera
    for i,(loc,power,size) in enumerate((
        ((-3,-3,-4),260,5),((4,2,-3),180,4))):
        light_data = bpy.data.lights.new(f"{name}/softbox{i}", "AREA")
        light_data.energy = power
        light_data.shape = "DISK"
        light_data.size = size
        light = bpy.data.objects.new(light_data.name, light_data)
        scene.collection.objects.link(light)
        light.location = loc
        light.rotation_euler = (Vector(target)-light.location).to_track_quat('-Z','Y').to_euler()
    return scene


def text(scene, name, body, location, size=0.11, color=(0.75,0.81,0.83)):
    font = bpy.data.curves.new(name, "FONT")
    font.body = body
    font.size = size
    font.extrude = 0.0003
    obj = bpy.data.objects.new(name, font)
    scene.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (math.pi,0,0)
    font.materials.append(material(f"Type/{color}", color,0,0.9))
    return obj


def backdrop(scene, dims=(8,6), z=0.27):
    cube(scene, f"{scene.name}/neutral display panel", (0,0,z),
         (dims[0],dims[1],0.02), material("Studio slate",(0.035,0.052,0.061),0,0.95))


def render(scene):
    if os.environ.get("GDP_FIXTURE_SKIP_RENDERS") == "1":
        return
    bpy.context.window.scene = scene
    bpy.ops.render.render(write_still=True, scene=scene.name)
    result = Path(scene.render.filepath)
    if not result.exists() or result.stat().st_size < 10000:
        raise RuntimeError(f"Render missing or too small: {result}")
    print(f"RENDER {result} {result.stat().st_size} bytes")


themes = ("Industrial","Storage","Maintenance","Laboratory","Cafeteria",
          "Security","Office","Boss / Arena","Central Hub")
family = new_scene("Family board", (2000,1500),8.2)
backdrop(family, (8.35,6.25))
text(family,"Family heading","LIGHTING / CEILING FIXTURE  -  FAMILY V2",(-3.48,-2.73,-0.30),0.20)
text(family,"Family subheading","ONE INDUSTRIAL PLATFORM   /   NINE ROOM ADAPTATIONS   /   USED CONDITION   /   ON",(-3.48,-2.50,-0.30),0.085)
for index,theme in enumerate(themes):
    col,row = index%3,index//3
    x,y = -2.37+col*2.37,-1.72+row*1.72
    add_fixture(family,theme,(x,y,0))
    text(family,f"Label {theme}",theme.upper(),(x-0.60,y+0.45,-0.29),0.13)
    text(family,f"Subtitle {theme}",("REINFORCED OVERLAY" if theme=="Boss / Arena" else
         "AXIAL OVERLAY" if theme=="Central Hub" else "ROOM THEME"),
         (x-0.60,y+0.63,-0.29),0.07,(0.46,0.58,0.62))
render(family)

conditions = new_scene("Condition board",(1800,700),8.25)
backdrop(conditions,(8.25,3.15))
text(conditions,"Condition heading","ONE MODEL  /  MATERIAL CONDITION",(-3.45,-1.25,-0.30),0.19)
for x,condition in zip((-2.38,0,2.38),("Maintained","Used","Worn")):
    add_fixture(conditions,"Industrial",(x,0,0),condition)
    text(conditions,f"Condition {condition}",condition.upper(),(x-0.58,0.46,-0.30),0.14)
    text(conditions,f"Condition note {condition}","SAME LENS / ON",(x-0.58,0.67,-0.30),0.08,(0.48,0.62,0.66))
render(conditions)

states = new_scene("Emission state board",(1800,700),8.25)
backdrop(states,(8.25,3.15))
text(states,"State heading","ONE MODEL  /  INDEPENDENT EMISSION STATE",(-3.45,-1.25,-0.30),0.19)
for x,state in zip((-2.38,0,2.38),("ON","OFF","ALERT")):
    add_fixture(states,"Industrial",(x,0,0),"Used",state)
    text(states,f"State {state}",state,(x-0.58,0.46,-0.30),0.14)
    text(states,f"State note {state}","USED HOUSING",(x-0.58,0.67,-0.30),0.08,(0.48,0.62,0.66))
render(states)

detail = new_scene("Structure closeup",(1600,1000),1.45,
                   camera_xyz=(1.1,-1.25,-1.8),target=(0,0,0))
backdrop(detail,(3.2,2.7),z=0.28)
add_fixture(detail,"Storage",(0,0,0),"Used")
render(detail)

game = new_scene("Gameplay scale",(1600,1000),4.3,
                 camera_xyz=(1.15,-3.25,1.62),target=(0,0,2.61))
wall = material("Game scale/neutral walls",(0.19,0.22,0.23),0.15,0.82)
floor = material("Game scale/neutral floor",(0.11,0.13,0.14),0.26,0.8)
cube(game,"Game scale/ceiling, no painted lamps",(0,0,2.88),(5,5,0.08),wall)
cube(game,"Game scale/floor",(0,0,-0.03),(5,5,0.06),floor)
for sx in (-1,1):
    cube(game,f"Game scale/side wall {sx}",(sx*2.5,0,1.42),(0.06,5,2.84),wall)
cube(game,"Game scale/far wall",(0,2.5,1.42),(5,0.06,2.84),wall)
add_fixture(game,"Storage",(0,0,2.75),"Used")
render(game)

# The gallery is the default editable scene when the file opens.
bpy.context.window.scene = family
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "ceiling_fixture_family_v2.blend"))
print("DIMENSIONS common housing: 1.20 x 0.28-0.30 x 0.09-0.12 m")
print("SCENES",len(bpy.data.scenes),"FIXTURE HOUSINGS",
      len([o for o in bpy.data.objects if o.get('fixture_housing')]))

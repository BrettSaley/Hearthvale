"""Sets up base building: what you can build, and stone to build with.

- Stone (an item): a bonus find while mining any ore rock
- M_BuildGhost: the see-through green/red preview in build mode
- Build pieces in /Game/Building/Pieces (UBuildPieceDefinition): wooden and
  stone floors, foundations and walls, a window wall, a door, stairs, a roof and gable ends, a fence, log poles and beams, a chest, a bed (where you wake), a table and stool, a hearth (an indoor fire), the crafting stations, a torch
- Farm machines (built with bars; each needs a Smithing level): sprinklers that speed crops up, an
  auto-feeder, harvesters that pick and replant plots, a collector that shears sheep and gathers eggs,
  an incubator that hatches chicks. Harvesters and collectors unload into the nearest chest in range.
  Logs are their fuel (better logs last longer).
- The Boats tab (each needs a Fishing level; built on the water, anywhere): a log raft for lakes
  and the shallows, a rowboat with a hold, and a fishing skiff with a wool sail that goes anywhere;
  and a crab pot, which catches crabs (lobsters out deep) with bait while you're away
- The Farm tab: a garden plot (a farm patch you build), a fence and gate (animals can't get through), a nest box (hens lay in it), a feeding trough (tame animals eat from it), a wool rug

Run after create_crafting.py, create_world.py, create_farming.py (for wool) and create_fishing.py (the stations' looks come
from their definitions), headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_building.py"

Pieces are always rewritten from the table below, so edit them here. An asset
name is the piece's ID in saves: don't rename one people may have built.
"""

import math
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
V = unreal.Vector
Snap = unreal.BuildSnap
Category = unreal.BuildCategory
FOLDER = "/Game/Building/Pieces"


def create(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path), False
    return asset_tools.create_asset(name, folder, asset_class, factory), True


def save(asset):
    assets.save_loaded_asset(asset, only_if_is_dirty=False)


def item(name):
    found = unreal.load_asset(f"/Game/Items/{name}")
    if found is None:
        raise RuntimeError(f"No item {name}.")
    return found


# --- Stone, from mining ---

def create_stone():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
    stone, made = create("Stone", "/Game/Items", unreal.ItemDefinition, factory)
    if made:
        stone.set_editor_property("display_name", unreal.Text("Stone"))
        stone.set_editor_property("examine", unreal.Text("A good flat stone. Build with it."))
        stone.set_editor_property("category", unreal.ItemCategory.RESOURCE)
        stone.set_editor_property("stackable", True)
        stone.set_editor_property("value", 1)
        save(stone)
    # Every ore rock gives some stone now and then.
    for rock in ["CopperRock", "IronRock", "SilverRock"]:
        node = unreal.load_asset(f"/Game/Gathering/Nodes/{rock}")
        drops = [d for d in node.get_editor_property("bonus_drops") if d.get_editor_property("item") != stone]
        drop = unreal.BonusDrop()
        drop.set_editor_property("item", stone)
        drop.set_editor_property("one_in", 2)
        drop.set_editor_property("quantity", 2)
        drops.append(drop)
        node.set_editor_property("bonus_drops", drops)
        save(node)


# --- The preview's material: unlit and see-through, its colour set in code ---

def create_ghost_material():
    ghost, made = create("M_BuildGhost", "/Game/Building", unreal.Material, unreal.MaterialFactoryNew())
    if made:
        ghost.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        ghost.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        color = mel.create_material_expression(ghost, unreal.MaterialExpressionVectorParameter, -400, 0)
        color.set_editor_property("parameter_name", "Color")
        color.set_editor_property("default_value", unreal.LinearColor(0.2, 1.0, 0.35, 1.0))
        opacity = mel.create_material_expression(ghost, unreal.MaterialExpressionConstant, -400, 200)
        opacity.set_editor_property("r", 0.38)
        mel.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
        mel.recompile_material(ghost)
        save(ghost)


# --- Surfaces: the Pack_Bonus textures, tinted (the pack's own materials are bright and untintable) ---

def texture(pack, kind):
    return unreal.load_asset(f"/Game/Pack_Bonus/Textures/{pack}/T_Pack_Bonus_{pack}_{kind}")


def create_surface_material():
    """M_BuildSurface: Base x Tint, with Normal and Roughness maps, tiled by Tiling."""
    m, made = create("M_BuildSurface", "/Game/Building", unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)  # Rebuilt every run, so changes here take.
    coords = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    tiling = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 120)
    tiling.set_editor_property("parameter_name", "Tiling")
    tiling.set_editor_property("default_value", 1.0)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -700, 40)
    mel.connect_material_expressions(coords, "", uv, "A")
    mel.connect_material_expressions(tiling, "", uv, "B")
    samples = {}
    for i, (name, kind, sampler) in enumerate([("Base", "basecolor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
                                               ("Normal", "normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
                                               ("Rough", "roughness", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)]):
        sample = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -450, -200 + i * 260)
        sample.set_editor_property("parameter_name", name)
        sample.set_editor_property("texture", texture("Wooden_Floor_2", kind))
        sample.set_editor_property("sampler_type", sampler)
        mel.connect_material_expressions(uv, "", sample, "UVs")
        samples[name] = sample
    tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -450, -420)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    tinted = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, -260)
    mel.connect_material_expressions(samples["Base"], "RGB", tinted, "A")
    mel.connect_material_expressions(tint, "", tinted, "B")
    mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(samples["Normal"], "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(samples["Rough"], "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    save(m)
    return m


# Name: (texture pack, tint, tiling). Always rewritten, so tweak the colours here.
SURFACES = {
    "Planks": ("Wooden_Floor_1", (0.62, 0.46, 0.36), 1.0),
    "WallPlanks": ("Wooden_Floor_2", (0.5, 0.36, 0.26), 1.0),
    "RoofPlanks": ("Wooden_Floor_3", (0.3, 0.25, 0.22), 2.0),
}


def create_surfaces():
    parent = create_surface_material()
    made = {}
    for name, (pack, tint, tiling) in SURFACES.items():
        mi, _ = create(f"MI_Build{name}", "/Game/Building", unreal.MaterialInstanceConstant,
                       unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        mel.set_material_instance_texture_parameter_value(mi, "Base", texture(pack, "basecolor"))
        mel.set_material_instance_texture_parameter_value(mi, "Normal", texture(pack, "normal"))
        mel.set_material_instance_texture_parameter_value(mi, "Rough", texture(pack, "roughness"))
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint, 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Tiling", tiling)
        save(mi)
        made[name] = mi
    return made


GABLE_HEIGHT = 200 * math.tan(math.radians(30))  # The roof's rise across a 2 m cell.


def create_gable_mesh():
    """SM_BuildGable: a 2 m long, 20 cm thick right triangle rising towards +X like a roof; two (one turned) fill a gable end."""
    path = "/Game/Building/SM_BuildGable"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)  # Rebuilt every run.
    mesh = unreal.DynamicMesh()
    corners = [unreal.Vector2D(-100, 0), unreal.Vector2D(100, 0), unreal.Vector2D(100, GABLE_HEIGHT)]
    # Drawn flat, then stood up: rolling -90 degrees turns its Y into height and its extrusion into -Y.
    stand = unreal.Transform(V(0, 10, 0), unreal.Rotator(-90, 0, 0), V(1, 1, 1))
    mesh = unreal.GeometryScript_Primitives.append_simple_extrude_polygon(mesh, unreal.GeometryScriptPrimitiveOptions(), stand, corners, 20.0)
    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property("enable_collision", True)
    options.set_editor_property("enable_nanite", False)
    built, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, path, options)
    save(built)
    return built


# --- Looks ---

SHAPES = {s: unreal.load_asset(f"/Engine/BasicShapes/{s}") for s in ["Cylinder", "Sphere", "Cone", "Cube"]}
SHAPES["Gable"] = create_gable_mesh()
MATERIALS = {
    "Flagstone": unreal.load_asset("/Game/Pack_Bonus/Materials/M_Pack_Bonus_Stone_1"),
    "Masonry": unreal.load_asset("/Game/Pack_Bonus/Materials/M_Pack_Bonus_Wall_1"),
    "Bark": unreal.load_asset("/Game/Gathering/Materials/MI_Bark"),
    "Flame": unreal.load_asset("/Game/Crafting/Materials/M_Flame"),
}
MATERIALS.update(create_surfaces())


def create_cloth():
    """Plain colours for cloth and the machines' metal (the basic shape material, tinted)."""
    parent = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    for name, color in {"Blanket": (0.32, 0.06, 0.045), "Linen": (0.72, 0.66, 0.55), "Straw": (0.62, 0.48, 0.18),
                        "Wool": (0.8, 0.77, 0.7), "Copper": (0.72, 0.36, 0.18), "Iron": (0.2, 0.2, 0.22),
                        "Steel": (0.55, 0.58, 0.63), "Water": (0.25, 0.5, 0.85)}.items():
        mi, _ = create(f"MI_Build{name}", "/Game/Building", unreal.MaterialInstanceConstant,
                       unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color, 1.0))
        save(mi)
        MATERIALS[name] = mi


create_cloth()


def part(shape, material, location, scale, rotation=(0, 0, 0), collides=True, visible=True):
    """Basic shapes are 100 cm across, centred on their origin. Rotation is (roll, pitch, yaw)."""
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", MATERIALS[material])
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    p.set_editor_property("collides", collides)
    p.set_editor_property("visible", visible)
    return p


def posts():
    """Log posts at a wall's ends, so corners look jointed."""
    return [part("Cylinder", "Bark", (x, 0, 100), (0.26, 0.26, 2.0), collides=False) for x in (-100, 100)]


def wall(material, thickness):
    return [part("Cube", material, (0, 0, 100), (2.0, thickness, 2.0))]


def door_frame():
    # Jambs, lintel and threshold-free gap 80 cm wide; the panel (index 3) swings on its left edge.
    return [part("Cube", "WallPlanks", (-70, 0, 100), (0.6, 0.2, 2.0)),
            part("Cube", "WallPlanks", (70, 0, 100), (0.6, 0.2, 2.0)),
            part("Cube", "WallPlanks", (0, 0, 185), (0.8, 0.2, 0.3)),
            part("Cube", "Planks", (0, 0, 85), (0.8, 0.08, 1.7))] + posts()


def roof():
    # A 30 degree slope across the cell, rising towards +X; two facing each other make a gable.
    return [part("Cube", "RoofPlanks", (0, 0, 58), (2.36, 2.12, 0.1), (0, 30, 0)),
            part("Cylinder", "Bark", (100, 0, 115), (0.2, 0.2, 2.1), (90, 0, 0), collides=False)]


def window_wall():
    # A wall with an 80 cm square window at eye height.
    return [part("Cube", "WallPlanks", (0, 0, 45), (2.0, 0.2, 0.9)),
            part("Cube", "WallPlanks", (0, 0, 175), (2.0, 0.2, 0.5)),
            part("Cube", "WallPlanks", (-70, 0, 120), (0.6, 0.2, 0.6)),
            part("Cube", "WallPlanks", (70, 0, 120), (0.6, 0.2, 0.6)),
            part("Cube", "Bark", (0, 0, 91), (0.84, 0.26, 0.04), collides=False)] + posts()


STAIR_RISE = 220  # A floor's top to the next floor's top: a wall plus a floor.
STAIR_STEPS = 8


def stairs():
    # Solid steps rising towards +X across a cell, each low enough to walk up.
    run = 200 / STAIR_STEPS
    steps = []
    for i in range(STAIR_STEPS):
        top = STAIR_RISE * (i + 1) / STAIR_STEPS
        steps.append(part("Cube", "Planks", (-100 + run * (i + 0.5), 0, top / 2), (run / 100, 1.6, top / 100)))
    return steps + [part("Cube", "Bark", (0, y, STAIR_RISE / 2), (2.3, 0.08, 0.14), (0, 45, 0), collides=False) for y in (-82, 82)]


def fence():
    # An unseen panel fills the gaps between the rails, so animals stay penned.
    return ([part("Cylinder", "Bark", (x, 0, 55), (0.12, 0.12, 1.1)) for x in (-92, 0, 92)]
            + [part("Cube", "Planks", (0, 0, z), (1.95, 0.05, 0.12)) for z in (40, 88)]
            + [part("Cube", "Planks", (0, 0, 50), (1.95, 0.05, 1.0), visible=False)])


def gate():
    # Posts at the ends, and a plank gate between them (index 2) swinging on its left edge.
    return [part("Cylinder", "Bark", (x, 0, 55), (0.12, 0.12, 1.1)) for x in (-92, 92)] + [
        part("Cube", "Planks", (0, 0, 52), (1.68, 0.05, 0.84))]


def garden_plot():
    # A low log frame round the soil (the soil itself is the farm patch the piece makes).
    return ([part("Cylinder", "Bark", (0, y, 7), (0.14, 0.14, 2.0), (0, 90, 0)) for y in (-94, 94)]
            + [part("Cylinder", "Bark", (x, 0, 7), (0.14, 0.14, 1.74), (90, 0, 0)) for x in (-94, 94)])


def nest_box():
    # An open plank box with straw in it.
    return ([part("Cube", "Planks", (0, 0, 4), (0.6, 0.5, 0.08))]
            + [part("Cube", "WallPlanks", (x, 0, 18), (0.04, 0.5, 0.3)) for x in (-28, 28)]
            + [part("Cube", "WallPlanks", (0, y, 18), (0.6, 0.04, 0.3)) for y in (-23, 23)]
            + [part("Sphere", "Straw", (0, 0, 14), (0.52, 0.42, 0.14), collides=False)])


def trough():
    # A long open plank box on legs, with feed in it.
    return ([part("Cube", "Planks", (0, 0, 22), (1.4, 0.5, 0.06))]
            + [part("Cube", "WallPlanks", (x, 0, 34), (0.04, 0.5, 0.3)) for x in (-68, 68)]
            + [part("Cube", "WallPlanks", (0, y, 34), (1.4, 0.04, 0.3)) for y in (-23, 23)]
            + [part("Cube", "Bark", (x, y, 10), (0.08, 0.08, 0.2)) for x in (-60, 60) for y in (-18, 18)]
            + [part("Sphere", "Straw", (0, 0, 32), (1.3, 0.42, 0.12), collides=False)])


def sprinkler(metal, height):
    # A pipe up from the ground with a head of four arms and a blue spray.
    return ([part("Cylinder", metal, (0, 0, height / 2), (0.08, 0.08, height / 100)),
             part("Sphere", metal, (0, 0, height + 4), (0.18, 0.18, 0.18), collides=False)]
            + [part("Cylinder", metal, (0, 0, height + 4), (0.04, 0.04, 0.7), rot, collides=False) for rot in ((90, 0, 0), (0, 90, 0))]
            + [part("Cone", "Water", (0, 0, height + 22), (0.5, 0.5, 0.3), collides=False)])


def harvester(metal, trim, s):
    # A metal box on wheels with a cutter in front and a chimney behind; s scales it.
    return ([part("Cube", metal, (0, 0, 60 * s), (1.3 * s, 0.9 * s, 0.9 * s)),
             part("Cube", "Planks", (0, 0, 108 * s), (1.2 * s, 0.85 * s, 0.08)),
             part("Cube", trim, (78 * s, 0, 22 * s), (0.12, 1.0 * s, 0.34 * s)),
             part("Cylinder", metal, (-48 * s, 25 * s, 140 * s), (0.14, 0.14, 0.7 * s), collides=False)]
            + [part("Cylinder", "Iron", (x * s, y * s, 22 * s), (0.44 * s, 0.44 * s, 0.08), (90, 0, 0), collides=False)
               for x in (-42, 42) for y in (-48, 48)])


def collector():
    # A steel drum with a funnel on top.
    return [part("Cylinder", "Steel", (0, 0, 60), (0.9, 0.9, 1.2)),
            part("Cone", "Steel", (0, 0, 140), (1.1, 1.1, 0.45), (180, 0, 0), collides=False),
            part("Cylinder", "Copper", (0, 0, 30), (0.94, 0.94, 0.08), collides=False),
            part("Cylinder", "Copper", (0, 0, 90), (0.94, 0.94, 0.08), collides=False)]


def feeder():
    # A big iron hopper on legs over a long trough.
    return ([part("Cube", "Planks", (0, 0, 22), (1.9, 0.9, 0.1)),
             part("Cube", "Iron", (0, 0, 95), (1.6, 0.7, 0.8)),
             part("Cone", "Iron", (0, 0, 48), (0.9, 0.6, 0.25), (180, 0, 0), collides=False)]
            + [part("Cylinder", "Iron", (x, y, 50), (0.07, 0.07, 1.0)) for x in (-75, 75) for y in (-30, 30)])


def incubator():
    # A copper-bound box with a glass-ish dome and a little chimney.
    return [part("Cube", "Copper", (0, 0, 35), (0.9, 0.7, 0.7)),
            part("Sphere", "Straw", (0, 0, 72), (0.7, 0.55, 0.35), collides=False),
            part("Cylinder", "Iron", (-30, 20, 95), (0.08, 0.08, 0.5), collides=False)]


def raft():
    # Five logs side by side, lashed with two planks.
    return ([part("Cylinder", "Bark", (0, y, 14), (0.3, 0.3, 2.6), (0, 90, 0)) for y in (-64, -32, 0, 32, 64)]
            + [part("Cube", "Planks", (x, 0, 30), (0.25, 1.7, 0.06), collides=False) for x in (-90, 90)])


def hull(length, width, wood, trim):
    """An open boat along +X: a flat bottom, straight sides, a square stern and a pointed bow (8 parts of a piece's 12)."""
    a, b = length / 2, width / 2
    nose = 0.25 * length
    straight = length - nose
    middle = (-a + a - nose) / 2
    slant = math.hypot(nose, b)
    angle = math.degrees(math.atan2(b, nose))
    return ([part("Cube", wood, (middle, 0, 10), (straight / 100, width / 100, 0.1)),
             part("Cube", wood, (-a, 0, 30), (0.08, (width + 8) / 100, 0.4))]
            + [part("Cube", wood, (middle, y, 30), (straight / 100, 0.08, 0.4)) for y in (-b, b)]
            + [part("Cube", wood, (a - nose / 2, side * b / 2, 30), (slant / 100, 0.08, 0.4), (0, 0, -side * angle)) for side in (-1, 1)]
            + [part("Cube", trim, (middle, y, 51), (straight / 100, 0.12, 0.04), collides=False) for y in (-b, b)])


def rowboat():
    return (hull(340, 130, "Planks", "Copper")
            + [part("Cube", "WallPlanks", (x, 0, 34), (0.3, 1.2, 0.05), collides=False) for x in (-60, 50)]
            # The oars, out to either side.
            + [part("Cylinder", "Bark", (0, side * 100, 42), (0.05, 0.05, 2.0), (side * 72, 0, 0), collides=False) for side in (-1, 1)])


def skiff():
    return (hull(480, 180, "Planks", "Iron")
            + [part("Cube", "Iron", (-175, 0, 28), (0.6, 1.2, 0.36)),
               # A mast with a boom, and a big wool sail.
               part("Cylinder", "Bark", (130, 0, 230), (0.12, 0.12, 4.2), collides=False),
               part("Cylinder", "Bark", (55, 0, 95), (0.07, 0.07, 1.6), (0, 90, 0), collides=False),
               part("Cube", "Wool", (60, 0, 240), (1.4, 0.03, 2.8), collides=False)])


def crab_pot():
    # A copper float with a little flag, its line running down to an iron cage on the bottom.
    return [part("Sphere", "Copper", (0, 0, 4), (0.36, 0.36, 0.3)),
            part("Cylinder", "Iron", (0, 0, 32), (0.03, 0.03, 0.5), collides=False),
            part("Cube", "Blanket", (10, 0, 50), (0.2, 0.02, 0.14), collides=False),
            part("Cylinder", "Straw", (0, 0, -70), (0.02, 0.02, 1.5), collides=False),
            part("Cube", "Iron", (0, 0, -160), (0.8, 0.6, 0.45), collides=False)]


def rug():
    return [part("Cube", "Wool", (0, 0, 1), (2.4, 1.6, 0.02), collides=False),
            part("Cube", "Blanket", (0, 0, 1.5), (1.8, 1.0, 0.02), collides=False)]


def chest():
    # A plank box with a darker lid and two log bands.
    return ([part("Cube", "Planks", (0, 0, 25), (1.0, 0.6, 0.5)),
             part("Cube", "WallPlanks", (0, 0, 56), (1.04, 0.64, 0.12))]
            + [part("Cube", "Bark", (x, 0, 31), (0.06, 0.66, 0.62), collides=False) for x in (-32, 32)])


def bed():
    # A plank frame with a headboard at +X, a linen mattress and pillow, and a red blanket.
    return [part("Cube", "Planks", (0, 0, 14), (2.1, 1.1, 0.28)),
            part("Cube", "WallPlanks", (102, 0, 40), (0.08, 1.1, 0.8)),
            part("Cube", "Linen", (0, 0, 34), (1.96, 1.0, 0.14), collides=False),
            part("Cube", "Linen", (75, 0, 46), (0.34, 0.7, 0.12), collides=False),
            part("Cube", "Blanket", (-22, 0, 42), (1.5, 1.06, 0.06), collides=False)]


def hearth():
    # A raised square of flagstones with a stone kerb round the fire: an indoor campfire.
    kerb = [part("Cube", "Masonry", (x * 62, y * 62, 22), (0.16 if x else 1.4, 1.4 if x else 0.16, 0.2))
            for x, y in ((1, 0), (-1, 0), (0, 1), (0, -1))]
    logs = [part("Cylinder", "Bark", (0, 0, 22), (0.12, 0.12, 0.7), (90, 0, yaw), collides=False) for yaw in (30, 120)]
    return ([part("Cube", "Flagstone", (0, 0, 6), (1.4, 1.4, 0.12))] + kerb + logs
            + [part("Sphere", "Flame", (0, 0, 18), (0.45, 0.45, 0.1), collides=False),
               part("Cone", "Flame", (0, 0, 46), (0.36, 0.36, 0.55), collides=False)])


def table():
    return ([part("Cube", "Planks", (0, 0, 76), (1.6, 0.9, 0.08))]
            + [part("Cube", "WallPlanks", (x, y, 36), (0.1, 0.1, 0.72)) for x in (-70, 70) for y in (-37, 37)])


def stool():
    return ([part("Cylinder", "Planks", (0, 0, 44), (0.42, 0.42, 0.06))]
            + [part("Cylinder", "Bark", (x, y, 21), (0.06, 0.06, 0.42)) for x, y in ((14, 0), (-7, 12), (-7, -12))])


def gable():
    # Under one roof cell's slope, on the wall below it.
    return [part("Gable", "WallPlanks", (0, 0, 0), (1, 1, 1))]


def torch():
    return [part("Cylinder", "Bark", (0, 0, 80), (0.08, 0.08, 1.6)),
            part("Cylinder", "Bark", (0, 0, 158), (0.13, 0.13, 0.12), collides=False),
            part("Cone", "Flame", (0, 0, 176), (0.14, 0.14, 0.26), collides=False)]


def station_parts(name):
    return list(unreal.load_asset(f"/Game/Crafting/Stations/{name}").get_editor_property("parts"))


# --- The pieces ---
# Logs don't stack (a backpack holds 28), so wood costs stay low: a 2x2 hut is about 30 logs.
# ID: (name, category, snap, cost, parts, footprint half-size, height, sort, extras)
# Everything but a campfire and a workbench has to be built near a workbench (extras "workbench": False).
S = Category.STRUCTURE
C = Category.CRAFTING
F = Category.FURNITURE
A = Category.FARM
B = Category.BOATS
PIECES = {
    "WoodFloor": ("Wood floor", S, Snap.CELL, [("Logs", 2)], [part("Cube", "Planks", (0, 0, 10), (2.0, 2.0, 0.2))],
                  (100, 100, 10), 20, 0, {}),
    "WoodWall": ("Wood wall", S, Snap.EDGE, [("Logs", 2)], wall("WallPlanks", 0.2) + posts(), (88, 10, 100), 200, 1, {}),
    "WoodWindow": ("Window wall", S, Snap.EDGE, [("Logs", 2)], window_wall(), (88, 10, 100), 200, 2, {}),
    "WoodDoor": ("Wood door", S, Snap.EDGE, [("Logs", 3)], door_frame(), (88, 10, 100), 200, 3,
                 {"door_part": 3, "door_hinge": V(-40, 0, 0)}),
    "WoodStairs": ("Stairs", S, Snap.CELL, [("Logs", 3)], stairs(), (96, 80, STAIR_RISE / 2), STAIR_RISE, 4, {}),
    "WoodRoof": ("Roof", S, Snap.CELL, [("Logs", 2)], roof(), (96, 96, 58), 115, 5, {}),
    "WoodGable": ("Gable end", S, Snap.EDGE, [("Logs", 2)], gable(), (60, 10, 20), GABLE_HEIGHT, 6, {}),
    "StoneFloor": ("Stone floor", S, Snap.CELL, [("Stone", 6)], [part("Cube", "Flagstone", (0, 0, 15), (2.0, 2.0, 0.3))],
                   (100, 100, 15), 30, 7, {}),
    # A floor whose stonework runs a metre down, so it sits flush on a slope; only its top half checks for room.
    "StoneFoundation": ("Foundation", S, Snap.CELL, [("Stone", 8)],
                        [part("Cube", "Masonry", (0, 0, -45), (2.0, 2.0, 1.1)), part("Cube", "Flagstone", (0, 0, 15), (2.02, 2.02, 0.1))],
                        (100, 100, 10), 20, 7, {}),
    "StoneWall": ("Stone wall", S, Snap.EDGE, [("Stone", 6)], wall("Masonry", 0.3), (86, 15, 100), 200, 8, {}),
    "WoodFence": ("Fence", A, Snap.EDGE, [("Logs", 1)], fence(), (88, 6, 55), 110, 31, {}),
    "WoodGate": ("Gate", A, Snap.EDGE, [("Logs", 2)], gate(), (88, 6, 55), 110, 32,
                 {"door_part": 2, "door_hinge": V(-84, 0, 0)}),
    "GardenPlot": ("Garden plot", A, Snap.CELL, [("Logs", 2)], garden_plot(), (98, 98, 8), 14, 30,
                   {"plot": (176, 176)}),
    "NestBox": ("Nest box", A, Snap.FREE, [("Logs", 2)], nest_box(), (32, 27, 18), 36, 33, {"storage": 6, "nest": True}),
    "FeedTrough": ("Feeding trough", A, Snap.FREE, [("Logs", 3)], trough(), (72, 27, 25), 50, 34, {"storage": 9, "trough": True}),
    # Farm machines: as above, plus the Smithing level to build one, what it does and its workings.
    "CopperSprinkler": ("Copper sprinkler", A, Snap.FREE, [("CopperBar", 2), ("Logs", 1)], sprinkler("Copper", 100), (12, 12, 55), 120, 40,
                        {"smithing": 5, "machine": unreal.FarmMachine.SPRINKLER, "range": 500, "boost": 2.0,
                         "about": "Waters plots within 5 m: crops grow twice as fast."}),
    "AutoFeeder": ("Auto-feeder", A, Snap.FREE, [("IronBar", 2), ("Logs", 3)], feeder(), (95, 45, 65), 135, 41,
                   {"smithing": 10, "storage": 27, "trough": True, "range": 3500,
                    "about": "A big hopper: tame animals within 35 m eat from it when they get hungry."}),
    "Harvester": ("Harvester", A, Snap.FREE, [("IronBar", 4), ("CopperBar", 2), ("Logs", 2)], harvester("Iron", "Copper", 1.0),
                  (80, 55, 60), 120, 42,
                  {"smithing": 15, "storage": 18, "machine": unreal.FarmMachine.HARVESTER, "range": 700, "work": 2.0,
                   "about": "Harvests plots within 7 m into itself and replants them from its seeds. Burns logs; you get the Farming XP."}),
    "IronSprinkler": ("Iron sprinkler", A, Snap.FREE, [("IronBar", 3), ("CopperBar", 1)], sprinkler("Iron", 150), (14, 14, 80), 170, 43,
                      {"smithing": 20, "machine": unreal.FarmMachine.SPRINKLER, "range": 900, "boost": 4.0,
                       "about": "Waters plots within 9 m: crops grow four times as fast."}),
    "Collector": ("Collector", A, Snap.FREE, [("SteelBar", 3), ("IronBar", 2)], collector(), (50, 50, 80), 160, 44,
                  {"smithing": 25, "storage": 27, "nest": True, "machine": unreal.FarmMachine.COLLECTOR, "range": 1500, "work": 1.5,
                   "about": "Shears sheep and sweeps up eggs and wool within 15 m; hens lay into it. Burns logs; you get the Farming XP."}),
    "SteelHarvester": ("Steam harvester", A, Snap.FREE, [("SteelBar", 5), ("SilverBar", 1), ("IronBar", 2)],
                       harvester("Steel", "Steel", 1.4), (112, 77, 84), 168, 46,
                       {"smithing": 35, "storage": 36, "machine": unreal.FarmMachine.HARVESTER, "range": 1400, "work": 0.5,
                        "about": "Harvests and replants plots within 14 m, four times as fast as the iron one. Burns logs."}),
    "SteelSprinkler": ("Steel irrigator", A, Snap.FREE, [("SteelBar", 4), ("SilverBar", 2)], sprinkler("Steel", 220), (16, 16, 115), 240, 47,
                       {"smithing": 45, "machine": unreal.FarmMachine.SPRINKLER, "range": 1500, "boost": 8.0,
                        "about": "Waters plots within 15 m: crops grow eight times as fast."}),
    "Incubator": ("Incubator", A, Snap.FREE, [("CopperBar", 3), ("IronBar", 2), ("Logs", 2)], incubator(), (45, 35, 45), 90, 45,
                  {"smithing": 30, "storage": 9, "machine": unreal.FarmMachine.INCUBATOR, "range": 1500, "work": 30.0,
                   "hatch": "Chicken", "about": "Hatches a chick from an egg in it every 30 s. Burns logs."}),
    # Boats: as above, plus the Fishing level to build one, its top speed (cm/s), how deep the water may get (m, 0 for any)
    # and how much it adds to the chance of a catch. Built on the water; the hold is its storage.
    "Raft": ("Log raft", B, Snap.FREE, [("Logs", 10)], raft(), (130, 85, 18), 30, 60,
             {"fishing": 5, "workbench": False, "boat_speed": 300.0, "boat_depth": 6.0,
              "about": "Logs lashed together: steer it out to the deep water of the lakes. Too flimsy for water over 6 m deep."}),
    "Rowboat": ("Rowboat", B, Snap.FREE, [("OakPlank", 8), ("CopperBar", 3)], rowboat(), (190, 65, 25), 22, 61,
                {"fishing": 20, "workbench": False, "storage": 9, "boat_speed": 520.0, "boat_depth": 20.0, "boat_bonus": 0.03,
                 "about": "Quicker than a raft, with a 9-slot hold for your catch. Out to sea as far as 20 m deep."}),
    "Skiff": ("Fishing skiff", B, Snap.FREE, [("AshPlank", 10), ("IronBar", 4), ("Wool", 8)], skiff(), (240, 90, 30), 22, 62,
              {"fishing": 40, "workbench": False, "storage": 27, "boat_speed": 850.0, "boat_bonus": 0.06,
               "about": "A sailing boat with a 27-slot hold: fast, steady to fish from, and fit for the open sea."}),
    "CrabPot": ("Crab pot", B, Snap.FREE, [("IronBar", 2), ("Logs", 2)], crab_pot(), (25, 25, 18), 10, 63,
                {"fishing": 15, "workbench": False, "on_water": True, "boat_depth": 12.0, "storage": 12,
                 "machine": unreal.FarmMachine.CRAB_POT, "range": 100, "work": 20.0, "work_xp": 30,
                 "bait": "Bait", "catch": "Crab", "deep_catch": "Lobster",
                 "about": "Sink it in water up to 12 m deep and bait it: it catches a crab every 20 s while the bait lasts (lobsters too, over 6 m deep). You get the Fishing XP."}),
    "WoolRug": ("Wool rug", F, Snap.FREE, [("Wool", 4)], rug(), (120, 80, 2), 3, 25, {}),
    # Timber framing: an upright log anywhere, and a log lying along a cell's edge (on a wall, poles or the ground).
    "WoodPole": ("Log pole", S, Snap.FREE, [("Logs", 1)], [part("Cylinder", "Bark", (0, 0, 100), (0.22, 0.22, 2.0))],
                 (10, 10, 100), 200, 9, {}),
    "WoodBeam": ("Log beam", S, Snap.EDGE, [("Logs", 1)], [part("Cylinder", "Bark", (0, 0, 11), (0.22, 0.22, 2.0), (0, 90, 0))],
                 (88, 10, 10), 22, 9, {}),
    "Campfire": ("Campfire", C, Snap.FREE, [("Logs", 2), ("Stone", 4)], station_parts("Campfire"), (70, 70, 30), 30, 10,
                 {"station": "Campfire", "workbench": False}),
    "Hearth": ("Hearth", C, Snap.FREE, [("Stone", 10), ("Logs", 1)], hearth(), (70, 70, 25), 30, 10,
               {"station": "Campfire"}),
    "Workbench": ("Workbench", C, Snap.FREE, [("Logs", 6)], station_parts("Workbench"), (82, 42, 50), 95, 11,
                  {"station": "Workbench", "workbench": False}),
    "Furnace": ("Furnace", C, Snap.FREE, [("Stone", 12), ("Logs", 2)], station_parts("Furnace"), (66, 66, 120), 280, 12,
                {"station": "Furnace"}),
    "Anvil": ("Anvil", C, Snap.FREE, [("IronBar", 3), ("Logs", 2)], station_parts("Anvil"), (45, 30, 35), 70, 13,
              {"station": "Anvil"}),
    "Range": ("Range", C, Snap.FREE, [("Stone", 10), ("IronBar", 2)], station_parts("Range"), (57, 42, 50), 100, 14,
              {"station": "Range"}),
    "Torch": ("Torch", F, Snap.FREE, [("Logs", 1)], torch(), (10, 10, 80), 180, 20,
              {"light_intensity": 14.0, "light_offset": V(0, 0, 185)}),
    "WoodChest": ("Chest", F, Snap.FREE, [("Logs", 4)], chest(), (52, 32, 31), 62, 21, {"storage": 18}),
    "WoodTable": ("Table", F, Snap.FREE, [("Logs", 2)], table(), (80, 45, 40), 80, 23, {}),
    "WoodStool": ("Stool", F, Snap.FREE, [("Logs", 1)], stool(), (22, 22, 23), 47, 24, {}),
    "WoodBed": ("Bed", F, Snap.FREE, [("Logs", 4)], bed(), (105, 55, 25), 50, 22, {"bed": True}),
}


def create_pieces():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.BuildPieceDefinition)
    for piece_id, (name, category, snap, cost, parts, footprint, height, sort, extras) in PIECES.items():
        piece, _ = create(piece_id, FOLDER, unreal.BuildPieceDefinition, factory)
        amounts = []
        for item_id, count in cost:
            amount = unreal.ItemAmount()
            amount.set_editor_property("item", item(item_id))
            amount.set_editor_property("quantity", count)
            amounts.append(amount)
        for prop, value in [("display_name", unreal.Text(name)), ("category", category), ("snap", snap), ("cost", amounts),
                            ("parts", parts), ("footprint", V(*footprint)), ("height", float(height)), ("sort_order", sort),
                            ("door_part", extras.get("door_part", -1)), ("door_hinge", extras.get("door_hinge", V(0, 0, 0))),
                            ("light_intensity", extras.get("light_intensity", 0.0)),
                            ("light_offset", extras.get("light_offset", V(0, 0, 100))),
                            ("needs_workbench", extras.get("workbench", True)),
                            ("storage_slots", extras.get("storage", 0)),
                            ("is_bed", extras.get("bed", False)),
                            ("nest_box", extras.get("nest", False)),
                            ("trough", extras.get("trough", False)),
                            ("required_level", extras.get("smithing", extras.get("fishing", 1))),
                            ("boat_speed", extras.get("boat_speed", 0.0)),
                            ("boat_max_depth", extras.get("boat_depth", 0.0)),
                            ("boat_catch_bonus", extras.get("boat_bonus", 0.0)),
                            ("on_water", extras.get("on_water", False)),
                            ("work_xp", extras.get("work_xp", 0)),
                            ("trap_bait", item(extras["bait"]) if "bait" in extras else None),
                            ("trap_catch", item(extras["catch"]) if "catch" in extras else None),
                            ("trap_deep_catch", item(extras["deep_catch"]) if "deep_catch" in extras else None),
                            ("description", unreal.Text(extras.get("about", ""))),
                            ("machine", extras.get("machine", unreal.FarmMachine.NONE)),
                            ("range", float(extras.get("range", 0))),
                            ("growth_boost", extras.get("boost", 1.0)),
                            ("work_seconds", extras.get("work", 2.0)),
                            ("hatch_animal", unreal.load_asset(f"/Game/Farming/Animals/{extras['hatch']}") if "hatch" in extras else None),
                            ("plot_size", unreal.Vector2D(*extras.get("plot", (0, 0)))),
                            ("plot_soil", unreal.load_asset("/Game/Farming/Materials/MI_Soil") if "plot" in extras else None)]:
            piece.set_editor_property(prop, value)
        if "smithing" in extras:  # (No skill is the default, and Python can't name it.)
            piece.set_editor_property("required_skill", unreal.Skill.SMITHING)
        if "fishing" in extras:
            piece.set_editor_property("required_skill", unreal.Skill.FISHING)
        station = extras.get("station")
        piece.set_editor_property("station", unreal.load_asset(f"/Game/Crafting/Stations/{station}") if station else None)
        save(piece)
    unreal.log(f"create_building: wrote {len(PIECES)} pieces.")


# Logs are the machines' fuel: the better the wood, the more jobs a log runs.
FUEL = {"Logs": 10, "OakLogs": 25, "AshLogs": 50}


def set_fuel():
    for item_id, jobs in FUEL.items():
        log = item(item_id)
        log.set_editor_property("fuel_value", jobs)
        save(log)


create_stone()
set_fuel()
create_ghost_material()
create_pieces()

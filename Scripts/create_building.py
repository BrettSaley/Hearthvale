"""Sets up base building: what you can build, and stone to build with.

- Stone (an item): a bonus find while mining any ore rock
- M_BuildGhost: the see-through green/red preview in build mode
- Build pieces in /Game/Building/Pieces (UBuildPieceDefinition): wooden and
  stone floors and walls, a window wall, a door, stairs, a roof and gable ends, a fence, log poles and beams, a chest, a bed (where you wake), a table and stool, a hearth (an indoor fire), the crafting stations, a torch

Run after create_crafting.py and create_world.py (the stations' looks come
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
    """Plain-coloured cloth for the bed (the basic shape material, tinted)."""
    parent = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    for name, color in {"Blanket": (0.32, 0.06, 0.045), "Linen": (0.72, 0.66, 0.55)}.items():
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
    return ([part("Cylinder", "Bark", (x, 0, 55), (0.12, 0.12, 1.1)) for x in (-92, 0, 92)]
            + [part("Cube", "Planks", (0, 0, z), (1.95, 0.05, 0.12)) for z in (40, 88)])


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
    "StoneWall": ("Stone wall", S, Snap.EDGE, [("Stone", 6)], wall("Masonry", 0.3), (86, 15, 100), 200, 8, {}),
    "WoodFence": ("Fence", F, Snap.EDGE, [("Logs", 1)], fence(), (88, 6, 55), 110, 9, {}),
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
                            ("is_bed", extras.get("bed", False))]:
            piece.set_editor_property(prop, value)
        station = extras.get("station")
        piece.set_editor_property("station", unreal.load_asset(f"/Game/Crafting/Stations/{station}") if station else None)
        save(piece)
    unreal.log(f"create_building: wrote {len(PIECES)} pieces.")


create_stone()
create_ghost_material()
create_pieces()

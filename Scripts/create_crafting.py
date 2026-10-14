"""Sets up crafting: refining what you gather at stations. No skills involved.

- IA_UseItem (right mouse): light a fire with held logs
- Logs' burn times; fish as caught are raw (cook them)
- Station definitions in /Game/Crafting/Stations: Campfire (lit with logs),
  Range, Furnace, Anvil, Workbench
- Recipes in /Game/Crafting/Recipes
- A range, furnace, anvil and workbench in the town corner of Lvl_ThirdPerson

Run create_items.py first (for the refined items), then this, headless with
the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_crafting.py"

Recipes, stations and the item tweaks below are always written, so edit them
here (or in the editor and don't re-run this). Stations are only placed if the
level has none.
"""

import math
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
V = unreal.Vector
Station = unreal.CraftStation


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
        raise RuntimeError(f"No item {name}; run create_items.py first.")
    return found


# --- Input: right mouse uses the held item ---

def create_input():
    action, made = create("IA_UseItem", "/Game/Input/Actions", unreal.InputAction, unreal.InputAction_Factory())
    if made:
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        save(action)
        context = unreal.load_asset("/Game/Input/IMC_Hearthvale")
        for key_name in ["RightMouseButton", "Gamepad_FaceButton_Top"]:
            key = unreal.Key()
            key.set_editor_property("key_name", key_name)
            context.map_key(action, key)
        save(context)


# --- Item tweaks ---

BURN_SECONDS = {"Logs": 60.0, "OakLogs": 90.0, "AshLogs": 120.0}
RAW_FISH = {"Perch": "Raw perch", "Trout": "Raw trout", "Pike": "Raw pike"}


def update_items():
    for name, seconds in BURN_SECONDS.items():
        logs = item(name)
        logs.set_editor_property("burn_seconds", seconds)
        save(logs)
    for name, display in RAW_FISH.items():
        fish = item(name)
        fish.set_editor_property("display_name", unreal.Text(display))
        fish.set_editor_property("examine", unreal.Text(f"{display.split()[1].capitalize()}, fresh from the water. Cook it on a fire."))
        fish.set_editor_property("category", unreal.ItemCategory.RESOURCE)
        fish.set_editor_property("heal_amount", 0)
        save(fish)


# --- Materials ---

MATERIALS = {}


def create_materials():
    flame, made = create("M_Flame", "/Game/Crafting/Materials", unreal.Material, unreal.MaterialFactoryNew())
    if made:
        flame.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        color = unreal.MaterialEditingLibrary.create_material_expression(flame, unreal.MaterialExpressionConstant3Vector, -300, 0)
        color.set_editor_property("constant", unreal.LinearColor(12.0, 3.2, 0.5, 1.0))
        unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(flame)
        save(flame)
    MATERIALS["Flame"] = flame
    parent = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    for name, (r, g, b) in {"Brick": (0.32, 0.1, 0.06), "Iron": (0.05, 0.05, 0.055), "Wood": (0.45, 0.3, 0.15)}.items():
        mi, made = create(f"MI_{name}", "/Game/Crafting/Materials", unreal.MaterialInstanceConstant,
                          unreal.MaterialInstanceConstantFactoryNew())
        if made:
            unreal.MaterialEditingLibrary.set_material_instance_parent(mi, parent)
            unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(r, g, b, 1.0))
            save(mi)
        MATERIALS[name] = mi
    for name in ["Stone", "Bark"]:
        MATERIALS[name] = unreal.load_asset(f"/Game/Gathering/Materials/MI_{name}")


# The station looks below need the materials, so make them first.
create_input()
create_materials()

SHAPES = {s: unreal.load_asset(f"/Engine/BasicShapes/{s}") for s in ["Cylinder", "Sphere", "Cone", "Cube"]}


def part(shape, material, location, scale, rotation=(0, 0, 0)):
    """Basic shapes are 100 cm across, centred on their origin. Rotation is (roll, pitch, yaw)."""
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", MATERIALS[material])
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    return p


def campfire():
    stones = [part("Sphere", "Stone", (55 * math.cos(a), 55 * math.sin(a), 8), (0.32, 0.26, 0.2), (0, 0, math.degrees(a)))
              for a in [i * 2 * math.pi / 7 for i in range(7)]]
    logs = [part("Cylinder", "Bark", (0, 0, 10), (0.14, 0.14, 0.8), (90, 0, yaw)) for yaw in (30, 120)]
    return stones + logs + [part("Sphere", "Flame", (0, 0, 6), (0.55, 0.55, 0.12)), part("Cone", "Flame", (0, 0, 42), (0.42, 0.42, 0.65))]


# Every station: (display name, kind, prompt verb, working verb, parts, light intensity, light offset, flicker)
STATIONS = {
    "Campfire": ("Campfire", Station.FIRE, "Cook at", "Cooking", campfire(), 40.0, (0, 0, 60), 0.35),
    "Range": ("Range", Station.FIRE, "Cook at", "Cooking",
              [part("Cube", "Brick", (0, 0, 42), (1.1, 0.8, 0.84)), part("Cube", "Iron", (0, 0, 87), (1.16, 0.86, 0.06)),
               part("Cube", "Flame", (55.5, 0, 34), (0.03, 0.46, 0.26)), part("Cylinder", "Iron", (-38, 0, 160), (0.22, 0.22, 1.4)),
               part("Cylinder", "Iron", (15, 10, 101), (0.32, 0.32, 0.22))], 15.0, (80, 0, 40), 0.1),
    "Furnace": ("Furnace", Station.FURNACE, "Smelt at", "Smelting",
                [part("Cylinder", "Brick", (0, 0, 75), (1.3, 1.3, 1.5)), part("Cone", "Brick", (0, 0, 190), (1.1, 1.1, 0.8)),
                 part("Cylinder", "Iron", (0, 0, 250), (0.3, 0.3, 0.6)), part("Cube", "Flame", (62, 0, 45), (0.08, 0.55, 0.45))],
                30.0, (110, 0, 50), 0.2),
    "Anvil": ("Anvil", Station.ANVIL, "Smith at", "Smithing",
              [part("Cylinder", "Bark", (0, 0, 25), (0.55, 0.55, 0.5)), part("Cube", "Iron", (0, 0, 60), (0.75, 0.28, 0.2)),
               part("Cone", "Iron", (52, 0, 62), (0.16, 0.16, 0.32), (0, -90, 0)), part("Cube", "Iron", (0, 0, 54), (0.3, 0.2, 0.12))],
              0.0, (0, 0, 60), 0.0),
    "Workbench": ("Workbench", Station.WORKBENCH, "Work at", "Crafting",
                  [part("Cube", "Wood", (0, 0, 86), (1.6, 0.8, 0.08))]
                  + [part("Cylinder", "Wood", (x, y, 41), (0.09, 0.09, 0.82)) for x in (-70, 70) for y in (-32, 32)]
                  + [part("Cube", "Bark", (-40, 0, 100), (0.5, 0.2, 0.2)), part("Cube", "Iron", (55, 10, 93), (0.25, 0.08, 0.06))],
                  0.0, (0, 0, 60), 0.0),
}
DEFINITIONS = {}


def create_stations():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.CraftingStationDefinition)
    for asset_name, (name, kind, prompt, verb, parts, light, offset, flicker) in STATIONS.items():
        d, _ = create(asset_name, "/Game/Crafting/Stations", unreal.CraftingStationDefinition, factory)
        for prop, value in [("display_name", unreal.Text(name)), ("station", kind), ("prompt_verb", unreal.Text(prompt)),
                            ("action_verb", unreal.Text(verb)), ("parts", parts), ("light_intensity", light),
                            ("light_offset", V(*offset)), ("flicker", flicker)]:
            d.set_editor_property(prop, value)
        save(d)
        DEFINITIONS[asset_name] = d


# --- Recipes: name: (station, [(item, count)], output, output count, seconds, message) ---

RECIPES = {
    # Cooking (campfires and ranges)
    "CookPerch": (Station.FIRE, [("Perch", 1)], "GrilledPerch", 1, 1.8, "You grill the perch."),
    "CookTrout": (Station.FIRE, [("Trout", 1)], "GrilledTrout", 1, 1.8, "You grill the trout."),
    "CookPike": (Station.FIRE, [("Pike", 1)], "GrilledPike", 1, 1.8, "You grill the pike."),
    "BakePotato": (Station.FIRE, [("Potato", 1)], "BakedPotato", 1, 1.8, "You bake a potato."),
    "MakeOnionSoup": (Station.FIRE, [("Onion", 2)], "OnionSoup", 1, 2.4, "You make onion soup."),
    "MakeCabbageStew": (Station.FIRE, [("Cabbage", 1), ("Potato", 1)], "CabbageStew", 1, 2.4, "You make cabbage stew."),
    "MakePumpkinSoup": (Station.FIRE, [("Pumpkin", 1), ("Onion", 1)], "PumpkinSoup", 1, 2.4, "You make pumpkin soup."),
    "MakeFishStew": (Station.FIRE, [("Trout", 1), ("Potato", 1), ("Onion", 1)], "FishStew", 1, 3.0, "You make a fish stew."),
    # Smelting
    "SmeltCopper": (Station.FURNACE, [("CopperOre", 1)], "CopperBar", 1, 2.4, "You smelt a copper bar."),
    "SmeltIron": (Station.FURNACE, [("IronOre", 2)], "IronBar", 1, 2.4, "You smelt an iron bar."),
    "SmeltSteel": (Station.FURNACE, [("IronBar", 2)], "SteelBar", 1, 3.0, "You fold the iron into a steel bar."),
    "SmeltSilver": (Station.FURNACE, [("SilverOre", 1)], "SilverBar", 1, 2.4, "You smelt a silver bar."),
    # Woodwork and gem cutting
    "SawPlank": (Station.WORKBENCH, [("Logs", 1)], "Plank", 1, 1.8, "You saw a plank."),
    "SawOakPlank": (Station.WORKBENCH, [("OakLogs", 1)], "OakPlank", 1, 1.8, "You saw an oak plank."),
    "SawAshPlank": (Station.WORKBENCH, [("AshLogs", 1)], "AshPlank", 1, 1.8, "You saw an ash plank."),
    "CutJade": (Station.WORKBENCH, [("UncutJade", 1)], "Jade", 1, 2.4, "You cut the jade."),
    "CutAmber": (Station.WORKBENCH, [("UncutAmber", 1)], "Amber", 1, 2.4, "You cut the amber."),
    "CutSapphire": (Station.WORKBENCH, [("UncutSapphire", 1)], "Sapphire", 1, 2.4, "You cut the sapphire."),
    "CutRuby": (Station.WORKBENCH, [("UncutRuby", 1)], "Ruby", 1, 2.4, "You cut the ruby."),
    # Smithing: better tools are the payoff for refining
    "SmithCopperAxe": (Station.ANVIL, [("CopperBar", 1), ("Plank", 1)], "CopperAxe", 1, 3.0, "You hammer out a copper axe."),
    "SmithCopperPickaxe": (Station.ANVIL, [("CopperBar", 1), ("Plank", 1)], "CopperPickaxe", 1, 3.0, "You hammer out a copper pickaxe."),
    "SmithCopperSword": (Station.ANVIL, [("CopperBar", 2)], "CopperSword", 1, 3.0, "You hammer out a copper sword."),
    "SmithIronAxe": (Station.ANVIL, [("IronBar", 2), ("OakPlank", 1)], "IronAxe", 1, 3.0, "You hammer out an iron axe."),
    "SmithIronPickaxe": (Station.ANVIL, [("IronBar", 2), ("OakPlank", 1)], "IronPickaxe", 1, 3.0, "You hammer out an iron pickaxe."),
    "SmithIronSword": (Station.ANVIL, [("IronBar", 3)], "IronSword", 1, 3.0, "You hammer out an iron sword."),
    "SmithSteelAxe": (Station.ANVIL, [("SteelBar", 2), ("AshPlank", 1)], "SteelAxe", 1, 3.0, "You hammer out a steel axe."),
    "SmithSteelPickaxe": (Station.ANVIL, [("SteelBar", 2), ("AshPlank", 1)], "SteelPickaxe", 1, 3.0, "You hammer out a steel pickaxe."),
    "SmithSteelSword": (Station.ANVIL, [("SteelBar", 3)], "SteelSword", 1, 3.0, "You hammer out a steel sword."),
    "SmithSilverRing": (Station.ANVIL, [("SilverBar", 1)], "SilverRing", 1, 3.0, "You shape a silver ring."),
    "SmithJadeRing": (Station.ANVIL, [("SilverBar", 1), ("Jade", 1)], "JadeRing", 1, 3.0, "You set the jade in a silver ring."),
    "SmithAmberRing": (Station.ANVIL, [("SilverBar", 1), ("Amber", 1)], "AmberRing", 1, 3.0, "You set the amber in a silver ring."),
    "SmithSapphireRing": (Station.ANVIL, [("SilverBar", 1), ("Sapphire", 1)], "SapphireRing", 1, 3.0, "You set the sapphire in a silver ring."),
    "SmithRubyRing": (Station.ANVIL, [("SilverBar", 1), ("Ruby", 1)], "RubyRing", 1, 3.0, "You set the ruby in a silver ring."),
}


def create_recipes():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.RecipeDefinition)
    for order, (asset_name, (kind, inputs, output, count, seconds, message)) in enumerate(RECIPES.items()):
        recipe, _ = create(asset_name, "/Game/Crafting/Recipes", unreal.RecipeDefinition, factory)
        amounts = []
        for name, quantity in inputs:
            a = unreal.ItemAmount()
            a.set_editor_property("item", item(name))
            a.set_editor_property("quantity", quantity)
            amounts.append(a)
        for prop, value in [("station", kind), ("inputs", amounts), ("output", item(output)), ("output_quantity", count),
                            ("seconds", seconds), ("made_message", unreal.Text(message)), ("sort_order", order)]:
            recipe.set_editor_property(prop, value)
        save(recipe)


# --- Placing the town's stations ---

LEVEL = "/Game/ThirdPerson/Lvl_ThirdPerson"
# Around the farm in the x>0, y>0 corner, each facing the middle of the map.
PLACEMENT = [("Range", 300, 1050), ("Furnace", 650, 1150), ("Anvil", 1000, 1100), ("Workbench", 1200, 650)]


def place_stations():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()
    if any(isinstance(a, unreal.CraftingStation) for a in actors):
        unreal.log_warning("create_crafting: level already has crafting stations; not placing more.")
        return
    obstacles, taken = [], []
    for a in actors:
        if isinstance(a, (unreal.ResourceNode, unreal.FarmPatch)):
            loc = a.get_actor_location()
            taken.append((loc.x, loc.y))
            continue
        origin, extent = a.get_actor_bounds(False)
        if 20 < extent.x < 1500 and 20 < extent.y < 1500 and extent.z > 20:
            obstacles.append((origin, extent))

    def clear(x, y):
        return (abs(x) < 1700 and abs(y) < 1700
                and all(abs(x - o.x) > e.x + 130 or abs(y - o.y) > e.y + 130 for o, e in obstacles)
                and all((x - tx) ** 2 + (y - ty) ** 2 > 300 ** 2 for tx, ty in taken))

    offsets = [(0, 0)] + [(r * dx, r * dy) for r in range(100, 900, 100)
                          for dx, dy in [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)]]
    for name, sx, sy in PLACEMENT:
        spot = next(((sx + dx, sy + dy) for dx, dy in offsets if clear(sx + dx, sy + dy)), None)
        if spot is None:
            unreal.log_warning(f"create_crafting: no clear spot for the {name}.")
            continue
        x, y = spot
        taken.append(spot)
        yaw = math.degrees(math.atan2(-y, -x))
        station = subsystem.spawn_actor_from_class(unreal.CraftingStation, V(x, y, 0), unreal.Rotator(0, 0, yaw))
        station.set_editor_property("definition", DEFINITIONS[name])
        station.set_actor_label(name)
        station.set_folder_path("Crafting")
        unreal.log_warning(f"create_crafting: placed the {name} at ({x}, {y}).")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)


update_items()
create_stations()
create_recipes()
place_stations()

"""Sets up animal husbandry: farm animals, what they give and what it cooks into.

- Egg and Wool (items), and Fried egg and Omelette (cooked at a fire)
- Animals in /Game/Farming/Animals (UAnimalDefinition): a chicken that eats seeds
  and lays eggs, and a sheep that eats vegetables and grows wool to shear
- Wild chickens and sheep placed around the start of the island, to tame

Run after create_items.py, create_skilling.py and create_crafting.py, and before
create_building.py (the wool rug needs wool), headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_farming.py"

Animals and recipes are always rewritten from the tables below, so edit them
here. Wild animals are only placed if the level has none yet.
"""

import math
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
V = unreal.Vector
LEVEL = "/Game/World/Lvl_World"


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


# --- Items ---

ITEMS = {
    "Egg": dict(name="Egg", examine="Still warm.", category=unreal.ItemCategory.FOOD, stackable=True, value=6, heal=1),
    "Wool": dict(name="Wool", examine="Soft, greasy and smells of sheep.", category=unreal.ItemCategory.RESOURCE, stackable=True, value=12),
    "FriedEgg": dict(name="Fried egg", examine="Sunny side up.", category=unreal.ItemCategory.FOOD, value=12, heal=4),
    "Omelette": dict(name="Omelette", examine="Eggs and onion, folded over.", category=unreal.ItemCategory.FOOD, value=40, heal=9),
}


def create_items():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
    for item_id, data in ITEMS.items():
        made, _ = create(item_id, "/Game/Items", unreal.ItemDefinition, factory)
        for prop, value in [("display_name", unreal.Text(data["name"])), ("examine", unreal.Text(data["examine"])),
                            ("category", data["category"]), ("stackable", data.get("stackable", False)),
                            ("value", data["value"]), ("heal_amount", data.get("heal", 0))]:
            made.set_editor_property(prop, value)
        save(made)


# --- Recipes: name: ([(item, count)], output, seconds, message), all at a fire ---

RECIPES = {
    "FryEgg": ([("Egg", 1)], "FriedEgg", 1.8, "You fry an egg."),
    "MakeOmelette": ([("Egg", 2), ("Onion", 1)], "Omelette", 2.4, "You make an omelette."),
}


def create_recipes():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.RecipeDefinition)
    for order, (asset_name, (inputs, output, seconds, message)) in enumerate(RECIPES.items()):
        recipe, _ = create(asset_name, "/Game/Crafting/Recipes", unreal.RecipeDefinition, factory)
        amounts = []
        for name, quantity in inputs:
            a = unreal.ItemAmount()
            a.set_editor_property("item", item(name))
            a.set_editor_property("quantity", quantity)
            amounts.append(a)
        for prop, value in [("station", unreal.CraftStation.FIRE), ("inputs", amounts), ("output", item(output)),
                            ("output_quantity", 1), ("seconds", seconds), ("made_message", unreal.Text(message)),
                            ("sort_order", 50 + order)]:
            recipe.set_editor_property(prop, value)
        save(recipe)


# --- Looks: basic shapes in plain colours, facing +X with their feet at the origin ---

SHAPES = {name: unreal.load_asset(f"/Engine/BasicShapes/{name}") for name in ["Cube", "Sphere", "Cylinder", "Cone"]}
COLORS = {"Feathers": (0.85, 0.82, 0.74), "Beak": (0.75, 0.38, 0.05), "Comb": (0.6, 0.04, 0.03),
          "Fleece": (0.82, 0.79, 0.7), "Skin": (0.5, 0.42, 0.36), "Face": (0.06, 0.05, 0.045)}
MATERIALS = {}


def create_materials():
    parent = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    for name, color in COLORS.items():
        mi, _ = create(f"MI_Animal{name}", "/Game/Farming/Materials", unreal.MaterialInstanceConstant,
                       unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color, 1.0))
        save(mi)
        MATERIALS[name] = mi


def part(shape, material, location, scale, rotation=(0, 0, 0)):
    """Basic shapes are 100 cm across, centred on their origin. Rotation is (roll, pitch, yaw)."""
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", MATERIALS[material])
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    p.set_editor_property("collides", False)
    return p


# Head parts go last (from "first_head_part" on), so they dip together about the neck.
def chicken():
    return ([part("Sphere", "Feathers", (0, 0, 28), (0.36, 0.28, 0.3)),
             part("Cone", "Feathers", (-17, 0, 36), (0.14, 0.12, 0.18), (0, 40, 0))]
            + [part("Cylinder", "Beak", (0, y, 8), (0.025, 0.025, 0.16)) for y in (-6, 6)]
            + [part("Sphere", "Feathers", (15, 0, 46), (0.16, 0.14, 0.17)),
               part("Cone", "Beak", (24, 0, 45), (0.05, 0.05, 0.08), (0, -90, 0)),
               part("Cube", "Comb", (14, 0, 56), (0.08, 0.02, 0.06))]
            + [part("Cylinder", "Face", (16, y, 49), (0.025, 0.025, 0.01), (90, 0, 0)) for y in (-6.5, 6.5)])


# The sheep's fleece is its first part: shown only while there's wool to shear.
def sheep():
    return ([part("Sphere", "Fleece", (0, 0, 70), (1.12, 0.74, 0.7)),
             part("Sphere", "Skin", (0, 0, 68), (0.95, 0.5, 0.48))]
            + [part("Cylinder", "Face", (x, y, 22), (0.08, 0.08, 0.44)) for x in (-30, 30) for y in (-16, 16)]
            + [part("Sphere", "Face", (58, 0, 88), (0.3, 0.2, 0.24)),
               part("Sphere", "Face", (50, 0, 98), (0.18, 0.2, 0.14))]
            + [part("Cube", "Face", (50, y, 96), (0.06, 0.14, 0.04), (0, 0, y)) for y in (-12, 12)])


# ID: name, young, food, produce, laid, verb, look, sizes and timings
ANIMALS = {
    "Chicken": dict(name="Chicken", young="Chick", food=["PotatoSeed", "OnionSeed", "CabbageSeed", "PumpkinSeed"],
                    produce="Egg", laid=True, verb="Collect", parts=chicken, produce_part=-1,
                    head=(4, (10, 0, 38), 55.0, True),
                    tame_feedings=2, tame_xp=20, collect_xp=8, fed=240.0, produce_seconds=60.0, breed=120.0, grow=180.0,
                    max_nearby=10, speed=110.0, wild_roam=1200.0, tame_roam=450.0, bob=3.0, radius=25.0),
    "Sheep": dict(name="Sheep", young="Lamb", food=["Potato", "Cabbage", "Pumpkin"],
                  produce="Wool", laid=False, verb="Shear", parts=sheep, produce_part=0, quantity=2,
                  head=(6, (40, 0, 82), 50.0, False),
                  tame_feedings=3, tame_xp=40, collect_xp=15, fed=300.0, produce_seconds=120.0, breed=180.0, grow=300.0,
                  max_nearby=8, speed=90.0, wild_roam=1800.0, tame_roam=600.0, bob=2.0, radius=55.0),
}


def create_animals():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.AnimalDefinition)
    made = {}
    for animal_id, a in ANIMALS.items():
        animal, _ = create(animal_id, "/Game/Farming/Animals", unreal.AnimalDefinition, factory)
        for prop, value in [("display_name", unreal.Text(a["name"])), ("young_name", unreal.Text(a["young"])),
                            ("food", [item(f) for f in a["food"]]), ("produce", item(a["produce"])),
                            ("lays_produce", a["laid"]), ("collect_verb", unreal.Text(a["verb"])),
                            ("produce_quantity", a.get("quantity", 1)), ("produce_part", a["produce_part"]),
                            ("parts", a["parts"]()), ("tame_feedings", a["tame_feedings"]), ("tame_xp", a["tame_xp"]),
                            ("collect_xp", a["collect_xp"]), ("fed_seconds", a["fed"]),
                            ("produce_seconds", a["produce_seconds"]), ("breed_seconds", a["breed"]),
                            ("grow_seconds", a["grow"]), ("max_nearby", a["max_nearby"]), ("speed", a["speed"]),
                            ("wild_roam", a["wild_roam"]), ("tame_roam", a["tame_roam"]), ("bob", a["bob"]),
                            ("radius", a["radius"]), ("first_head_part", a["head"][0]),
                            ("head_pivot", V(*a["head"][1])), ("head_dip", a["head"][2]), ("pecks", a["head"][3])]:
            animal.set_editor_property(prop, value)
        save(animal)
        made[animal_id] = animal
    return made


# --- Wild ones by the start: a few of each, on dry ground in a ring round the camp ---

FLOCKS = [("Chicken", 2600, 20, 4), ("Sheep", 3800, 200, 4)]  # (animal, distance from the start, bearing, how many)


def place_wild(animals):
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if any(isinstance(a, unreal.FarmAnimal) for a in subsystem.get_all_level_actors()):
        unreal.log_warning("create_farming: the level already has animals; not placing more.")
        return
    height = unreal.WorldGenerator.get_ground_height
    placed = 0
    for animal_id, distance, bearing, count in FLOCKS:
        # Walk round the ring from the bearing until there's dry, gentle ground.
        for step in range(36):
            angle = math.radians(bearing + step * 10)
            cx, cy = distance * math.cos(angle), distance * math.sin(angle)
            if all(height(cx + dx, cy + dy) > 120 for dx in (-400, 400) for dy in (-400, 400)):
                break
        for i in range(count):
            x, y = cx + 150 * math.cos(i * 2.1), cy + 150 * math.sin(i * 2.1)
            actor = subsystem.spawn_actor_from_class(unreal.FarmAnimal, V(x, y, height(x, y)), unreal.Rotator(0, 0, i * 90))
            actor.set_editor_property("animal", animals[animal_id])
            actor.set_actor_label(f"Wild{animal_id}_{i}")
            actor.set_folder_path("Farming/Animals")
            placed += 1
        unreal.log(f"create_farming: {count} wild {animal_id} at ({cx:.0f}, {cy:.0f}).")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(f"create_farming: placed {placed} wild animals.")


create_items()
create_recipes()
create_materials()
place_wild(create_animals())

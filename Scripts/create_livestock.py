"""Sets up livestock: cows to milk, pigs that root up truffles, and a dairy.

- Items: Bucket (made at a workbench; you need one in your pack to milk a cow),
  Milk, Butter, Cheese and Truffle, and three dishes cooked with them at a fire
  (Buttered potato, Truffle omelette, Fish chowder)
- The Churn (a crafting station, /Game/Crafting/Stations/Churn) and the Butter
  churn build piece (Farm tab) that is one: milk into butter and cheese, for Farming XP
- Animals in /Game/Farming/Animals: a Cow that eats vegetables and is milked
  with a bucket, and a Pig that roots up truffles on the ground near it
- A few wild cows and pigs placed around the start of the island, to tame
- A Brush (made at a workbench from a plank and some wool): groom any tame
  animal once a day to keep it happy; happy animals give a little more

Run after create_farming.py and create_building.py (it reuses their colours and
the building piece setup), headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_livestock.py"

Everything but the wild animals is always rewritten from the tables below, so
edit them here. Wild ones are placed only for a kind the level has none of yet.
"""

import math
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
V = unreal.Vector
Station = unreal.CraftStation
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
        raise RuntimeError(f"No item {name}; run create_items.py and create_farming.py first.")
    return found


# --- Items ---

F = unreal.ItemCategory.FOOD
ITEMS = {
    "Bucket": dict(name="Bucket", examine="Planks and a rope handle. For milking.", category=unreal.ItemCategory.TOOL, value=15),
    "Brush": dict(name="Brush", examine="Stiff bristles on a wooden back. Animals love it.", category=unreal.ItemCategory.TOOL, value=12),
    "Milk": dict(name="Milk", examine="Fresh from the cow.", category=F, stackable=True, value=8, heal=2),
    "Butter": dict(name="Butter", examine="Yellow, salty and soft.", category=F, stackable=True, value=25, heal=3),
    "Cheese": dict(name="Cheese", examine="A wheel of hard cheese.", category=F, stackable=True, value=60, heal=6),
    "Truffle": dict(name="Truffle", examine="Knobbly, black and worth a fortune to a cook.",
                    category=unreal.ItemCategory.VALUABLE, stackable=True, value=90),
    "ButteredPotato": dict(name="Buttered potato", examine="Baked, split and buttered.", category=F, value=35, heal=8),
    "TruffleOmelette": dict(name="Truffle omelette", examine="Eggs folded over shavings of truffle.", category=F, value=220, heal=15),
    "FishChowder": dict(name="Fish chowder", examine="Trout and potato in a creamy broth.", category=F, value=80, heal=14),
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


# --- Recipes: name: (station, [(item, count)], output, count, seconds, message[, Farming XP]) ---

RECIPES = {
    "MakeBucket": (Station.WORKBENCH, [("Plank", 3)], "Bucket", 1, 2.4, "You make a bucket."),
    "MakeBrush": (Station.WORKBENCH, [("Plank", 1), ("Wool", 1)], "Brush", 1, 2.0, "You make a brush."),
    "ChurnButter": (Station.CHURN, [("Milk", 2)], "Butter", 1, 3.0, "You churn the milk into butter.", 12),
    "PressCheese": (Station.CHURN, [("Milk", 3)], "Cheese", 1, 3.6, "You press a cheese.", 20),
    "MakeButteredPotato": (Station.FIRE, [("Potato", 1), ("Butter", 1)], "ButteredPotato", 1, 2.0, "You bake and butter a potato."),
    "MakeTruffleOmelette": (Station.FIRE, [("Egg", 2), ("Truffle", 1)], "TruffleOmelette", 1, 2.4, "You make a truffle omelette."),
    "MakeFishChowder": (Station.FIRE, [("Trout", 1), ("Potato", 1), ("Milk", 1)], "FishChowder", 1, 3.0, "You make a fish chowder."),
}


def create_recipes():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.RecipeDefinition)
    for order, (asset_name, (station, inputs, output, count, seconds, message, *xp)) in enumerate(RECIPES.items()):
        recipe, _ = create(asset_name, "/Game/Crafting/Recipes", unreal.RecipeDefinition, factory)
        amounts = []
        for name, quantity in inputs:
            a = unreal.ItemAmount()
            a.set_editor_property("item", item(name))
            a.set_editor_property("quantity", quantity)
            amounts.append(a)
        for prop, value in [("station", station), ("inputs", amounts), ("output", item(output)),
                            ("output_quantity", count), ("seconds", seconds), ("made_message", unreal.Text(message)),
                            ("sort_order", 80 + order)]:
            recipe.set_editor_property(prop, value)
        if xp:
            recipe.set_editor_property("skill", unreal.Skill.FARMING)
            recipe.set_editor_property("xp", xp[0])
        save(recipe)


# --- Looks: basic shapes in plain colours, facing +X with their feet at the origin ---

SHAPES = {name: unreal.load_asset(f"/Engine/BasicShapes/{name}") for name in ["Cube", "Sphere", "Cylinder", "Cone"]}
# Dark on purpose, like create_farming.py's: auto exposure brightens them a lot.
COLORS = {"CowHide": (0.78, 0.76, 0.72), "CowSpots": (0.05, 0.04, 0.035), "Pink": (0.62, 0.32, 0.3),
          "PigSkin": (0.68, 0.42, 0.38), "Horn": (0.55, 0.5, 0.4)}
MATERIALS = {"Face": unreal.load_asset("/Game/Farming/Materials/MI_AnimalFace"),
             "Wood": unreal.load_asset("/Game/Crafting/Materials/MI_Wood"),
             "Iron": unreal.load_asset("/Game/Crafting/Materials/MI_Iron")}


def create_materials():
    parent = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    for name, color in COLORS.items():
        mi, _ = create(f"MI_Animal{name}", "/Game/Farming/Materials", unreal.MaterialInstanceConstant,
                       unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color, 1.0))
        save(mi)
        MATERIALS[name] = mi


def part(shape, material, location, scale, rotation=(0, 0, 0), collides=False):
    """Basic shapes are 100 cm across, centred on their origin. Rotation is (roll, pitch, yaw)."""
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", MATERIALS[material])
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    p.set_editor_property("collides", collides)
    return p


def eyes(x, half, z):
    return [part("Cylinder", "Face", (x, y, z), (0.03, 0.03, 0.01), (90, 0, 0)) for y in (-half, half)]


# Head parts go last (from "first_head_part" on), so they dip together about the neck.
def cow():
    return ([part("Sphere", "CowHide", (0, 0, 102), (1.6, 0.74, 0.62)),
             part("Sphere", "CowSpots", (25, 30, 110), (0.5, 0.16, 0.36)),
             part("Sphere", "CowSpots", (-40, -29, 104), (0.42, 0.16, 0.34)),
             part("Sphere", "CowSpots", (-10, 0, 132), (0.4, 0.3, 0.06))]
            + [part("Cylinder", "CowHide", (x, y, 38), (0.13, 0.13, 0.76)) for x in (-55, 55) for y in (-22, 22)]
            + [part("Sphere", "Pink", (-32, 0, 70), (0.26, 0.22, 0.16)),
               part("Cylinder", "CowSpots", (-84, 0, 92), (0.03, 0.03, 0.55), (0, 15, 0))]
            + [part("Cube", "CowHide", (98, 0, 128), (0.42, 0.36, 0.4)),
               part("Cube", "Pink", (122, 0, 115), (0.22, 0.32, 0.22))]
            + [part("Cone", "Horn", (92, y, 154), (0.06, 0.06, 0.18), (y * 1.5, 0, 0)) for y in (-15, 15)]
            + eyes(118, 17, 135))


def pig():
    return ([part("Sphere", "PigSkin", (0, 0, 50), (1.05, 0.62, 0.56))]
            + [part("Cylinder", "PigSkin", (x, y, 12), (0.09, 0.09, 0.24)) for x in (-30, 30) for y in (-16, 16)]
            + [part("Sphere", "Pink", (-52, 0, 60), (0.07, 0.07, 0.07)),
               part("Sphere", "PigSkin", (52, 0, 56), (0.42, 0.4, 0.38)),
               part("Cylinder", "Pink", (74, 0, 51), (0.17, 0.17, 0.08), (0, 90, 0))]
            + [part("Cone", "PigSkin", (50, y, 76), (0.09, 0.07, 0.12), (y * 2.0, -20, 0)) for y in (-11, 11)]
            + eyes(68, 10, 62))


# ID: name, young, food, produce, laid, nest, tool, verb, look, sizes and timings
ANIMALS = {
    "Cow": dict(name="Cow", young="Calf", food=["Cabbage", "Pumpkin", "Potato"],
                produce="Milk", laid=False, nest=False, tool="Bucket", verb="Milk", parts=cow, quantity=2,
                head=(10, (78, 0, 118), 45.0, False),
                tame_feedings=4, tame_xp=50, collect_xp=15, fed=300.0, produce_seconds=120.0, breed=240.0, grow=360.0,
                max_nearby=6, speed=80.0, wild_roam=2000.0, tame_roam=700.0, bob=2.0, radius=75.0),
    "Pig": dict(name="Pig", young="Piglet", food=["Potato", "Onion", "Pumpkin", "Cabbage"],
                produce="Truffle", laid=True, nest=False, tool=None, verb="Collect", parts=pig, quantity=1,
                head=(6, (38, 0, 56), 35.0, False),
                tame_feedings=3, tame_xp=35, collect_xp=20, fed=240.0, produce_seconds=150.0, breed=150.0, grow=240.0,
                max_nearby=8, speed=100.0, wild_roam=1600.0, tame_roam=600.0, bob=2.0, radius=50.0),
}


def create_animals():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.AnimalDefinition)
    made = {}
    for animal_id, a in ANIMALS.items():
        animal, _ = create(animal_id, "/Game/Farming/Animals", unreal.AnimalDefinition, factory)
        for prop, value in [("display_name", unreal.Text(a["name"])), ("young_name", unreal.Text(a["young"])),
                            ("food", [item(f) for f in a["food"]]), ("produce", item(a["produce"])),
                            ("lays_produce", a["laid"]), ("uses_nest", a["nest"]),
                            ("collect_tool", item(a["tool"]) if a["tool"] else None),
                            ("collect_verb", unreal.Text(a["verb"])),
                            ("produce_quantity", a["quantity"]), ("produce_part", -1),
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


# --- The churn: a crafting station, and the build piece that is one ---

def churn():
    return ([part("Cylinder", "Wood", (0, 0, 42), (0.42, 0.42, 0.84), collides=True)]
            + [part("Cylinder", "Iron", (0, 0, z), (0.44, 0.44, 0.04)) for z in (12, 72)]
            + [part("Cylinder", "Wood", (0, 0, 86), (0.36, 0.36, 0.05)),
               part("Cylinder", "Wood", (0, 0, 120), (0.04, 0.04, 0.7)),
               part("Cube", "Wood", (0, 0, 152), (0.24, 0.04, 0.04))])


def create_churn():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.CraftingStationDefinition)
    station, _ = create("Churn", "/Game/Crafting/Stations", unreal.CraftingStationDefinition, factory)
    for prop, value in [("display_name", unreal.Text("Butter churn")), ("station", Station.CHURN),
                        ("prompt_verb", unreal.Text("Churn at")), ("action_verb", unreal.Text("Churning")),
                        ("parts", churn()), ("light_intensity", 0.0)]:
        station.set_editor_property(prop, value)
    save(station)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.BuildPieceDefinition)
    piece, _ = create("ButterChurn", "/Game/Building/Pieces", unreal.BuildPieceDefinition, factory)
    cost = []
    for item_id, count in [("Logs", 3), ("CopperBar", 1)]:
        amount = unreal.ItemAmount()
        amount.set_editor_property("item", item(item_id))
        amount.set_editor_property("quantity", count)
        cost.append(amount)
    for prop, value in [("display_name", unreal.Text("Butter churn")), ("category", unreal.BuildCategory.FARM),
                        ("snap", unreal.BuildSnap.FREE), ("cost", cost), ("parts", churn()), ("footprint", V(22, 22, 80)),
                        ("height", 160.0), ("sort_order", 35), ("station", station),
                        ("description", unreal.Text("Churns milk into butter and presses cheese."))]:
        piece.set_editor_property(prop, value)
    save(piece)


# --- Wild ones by the start: a few of each kind the level doesn't have yet ---

HERDS = [("Cow", 4300, 110, 3), ("Pig", 3200, 300, 3)]  # (animal, distance from the start, bearing, how many)


def place_wild(animals):
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    present = {a.get_editor_property("animal").get_name() for a in subsystem.get_all_level_actors()
               if isinstance(a, unreal.FarmAnimal) and a.get_editor_property("animal")}
    height = unreal.WorldGenerator.get_ground_height
    placed = 0
    for animal_id, distance, bearing, count in HERDS:
        if animal_id in present:
            unreal.log(f"create_livestock: the level already has {animal_id}s.")
            continue
        # Walk round the ring from the bearing until there's dry, gentle ground.
        for step in range(36):
            angle = math.radians(bearing + step * 10)
            cx, cy = distance * math.cos(angle), distance * math.sin(angle)
            if all(height(cx + dx, cy + dy) > 120 for dx in (-400, 400) for dy in (-400, 400)):
                break
        for i in range(count):
            x, y = cx + 220 * math.cos(i * 2.1), cy + 220 * math.sin(i * 2.1)
            actor = subsystem.spawn_actor_from_class(unreal.FarmAnimal, V(x, y, height(x, y)), unreal.Rotator(0, 0, i * 90))
            actor.set_editor_property("animal", animals[animal_id])
            actor.set_actor_label(f"Wild{animal_id}_{i}")
            actor.set_folder_path("Farming/Animals")
            placed += 1
        unreal.log(f"create_livestock: {count} wild {animal_id} at ({cx:.0f}, {cy:.0f}).")
    if placed:
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(f"create_livestock: placed {placed} wild animals.")


def set_grooming():
    """Every farm animal, the chickens and sheep from create_farming.py too, is groomed with the brush."""
    brush = item("Brush")
    for name in ["Chicken", "Sheep", "Cow", "Pig"]:
        kind = unreal.load_asset(f"/Game/Farming/Animals/{name}")
        if kind:
            kind.set_editor_property("groom_tool", brush)
            save(kind)


create_items()
create_recipes()
create_materials()
create_churn()
animals = create_animals()
set_grooming()
place_wild(animals)

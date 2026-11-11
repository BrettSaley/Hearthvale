"""Sets up hunting: what wild animals drop, what it cooks into, and bows to hunt with.

- Meat (raw, cooked at a fire): rabbit, venison, pork, goat and bear
- Hides (deer, boars, goats) and pelts (foxes, wolves, bears): rugs, in create_building.py
- Bows (short, oak, ash), made at a workbench from planks and wool: they shoot
  from far off, an arrow a shot, and use Precision for damage as well as aim
- Arrows, fletched at a workbench from a plank and a stone

What each animal drops is in AWildlifeHerd::GetLoot; how hard they are to hit
and how much health they have, in its FightOf.

Run after create_items.py, create_crafting.py and create_farming.py (for wool),
and before create_building.py, headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_hunting.py"
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
Cat = unreal.ItemCategory
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
        raise RuntimeError(f"No item {name}.")
    return found


# Meat: raw ID: (raw name, examine, value, cooked ID, cooked name, examine, value, heals)
MEAT = {
    "RawRabbit": ("Raw rabbit", "A skinned rabbit. Cook it on a fire.", 4, "RoastRabbit", "Roast rabbit", "Small, but tasty.", 10, 5),
    "RawVenison": ("Raw venison", "Dark, lean deer meat. Cook it on a fire.", 15, "VenisonSteak", "Venison steak", "Rich and gamey.", 35, 9),
    "RawPork": ("Raw pork", "A hunk of wild boar. Cook it on a fire.", 15, "RoastPork", "Roast pork", "Crackling and all.", 32, 8),
    "RawGoat": ("Raw goat", "Tough mountain meat. Cook it on a fire.", 12, "RoastGoat", "Roast goat", "Chewy, but it fills you up.", 28, 8),
    "RawBearMeat": ("Raw bear meat", "A great slab of bear. Cook it on a fire.", 40, "BearSteak", "Bear steak", "Enough for a feast.", 80, 14),
}

ITEMS = {
    "Hide": dict(name="Hide", examine="A rough animal hide.", category=Cat.RESOURCE, stackable=True, value=15),
    "FoxPelt": dict(name="Fox pelt", examine="Soft red fur with a white-tipped tail.", category=Cat.RESOURCE, stackable=True, value=40),
    "WolfPelt": dict(name="Wolf pelt", examine="Thick grey fur. Warm, and hard won.", category=Cat.RESOURCE, stackable=True, value=80),
    "BearPelt": dict(name="Bear pelt", examine="A vast brown pelt. You earned this.", category=Cat.RESOURCE, stackable=True, value=200),
    "Arrows": dict(name="Arrows", examine="Flint-tipped and wool-fletched.", category=Cat.RESOURCE, stackable=True, value=1),
    # Bows: reach (cm), seconds a shot, accuracy and strength, and the Precision to use one.
    "Shortbow": dict(name="Shortbow", examine="A simple bow of sawn planks.", category=Cat.WEAPON, value=30,
                     bow=(2500.0, 3.0, 6, 6), requires=1),
    "OakLongbow": dict(name="Oak longbow", examine="Tall and strong: it shoots hard.", category=Cat.WEAPON, value=120,
                       bow=(2800.0, 3.0, 14, 12), requires=15),
    "AshLongbow": dict(name="Ash longbow", examine="Springy ash. The hunter's bow.", category=Cat.WEAPON, value=360,
                       bow=(3200.0, 2.6, 24, 18), requires=30),
}


def create_items():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
    entries = dict(ITEMS)
    for raw, (name, examine, value, cooked, cooked_name, cooked_examine, cooked_value, heal) in MEAT.items():
        entries[raw] = dict(name=name, examine=examine, category=Cat.RESOURCE, value=value)
        entries[cooked] = dict(name=cooked_name, examine=cooked_examine, category=Cat.FOOD, value=cooked_value, heal=heal)
    made_items = {}
    for item_id, data in entries.items():
        made, _ = create(item_id, "/Game/Items", unreal.ItemDefinition, factory)
        for prop, value in [("display_name", unreal.Text(data["name"])), ("examine", unreal.Text(data["examine"])),
                            ("category", data["category"]), ("stackable", data.get("stackable", False)),
                            ("value", data["value"]), ("heal_amount", data.get("heal", 0))]:
            made.set_editor_property(prop, value)
        made_items[item_id] = made
    # Bows last, once the arrows they shoot exist.
    for item_id, data in ITEMS.items():
        if "bow" not in data:
            continue
        reach, seconds, accuracy, strength = data["bow"]
        bow = made_items[item_id]
        for prop, value in [("equip_slot", unreal.EquipSlot.WEAPON), ("attack_range", reach), ("attack_seconds", seconds),
                            ("accuracy_bonus", accuracy), ("strength_bonus", strength), ("ammo", made_items["Arrows"]),
                            ("requirements", {unreal.Skill.PRECISION: data["requires"]} if data["requires"] > 1 else {})]:
            bow.set_editor_property(prop, value)
    for made in made_items.values():
        save(made)


# Recipes: name: (station, [(item, count)], output, count, seconds, message)
RECIPES = {f"Cook{raw}": (Station.FIRE, [(raw, 1)], data[3], 1, 2.4, f"You cook the {data[0].split(' ', 1)[1]}.")
           for raw, data in MEAT.items()}
RECIPES.update({
    "FletchArrows": (Station.WORKBENCH, [("Plank", 1), ("Stone", 1)], "Arrows", 15, 2.4, "You fletch fifteen arrows."),
    "MakeShortbow": (Station.WORKBENCH, [("Plank", 2), ("Wool", 1)], "Shortbow", 1, 3.0, "You make a shortbow."),
    "MakeOakLongbow": (Station.WORKBENCH, [("OakPlank", 2), ("Wool", 2)], "OakLongbow", 1, 3.0, "You make an oak longbow."),
    "MakeAshLongbow": (Station.WORKBENCH, [("AshPlank", 2), ("Wool", 2)], "AshLongbow", 1, 3.0, "You make an ash longbow."),
})


def create_recipes():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.RecipeDefinition)
    for order, (asset_name, (station, inputs, output, count, seconds, message)) in enumerate(RECIPES.items()):
        recipe, _ = create(asset_name, "/Game/Crafting/Recipes", unreal.RecipeDefinition, factory)
        amounts = []
        for name, quantity in inputs:
            a = unreal.ItemAmount()
            a.set_editor_property("item", item(name))
            a.set_editor_property("quantity", quantity)
            amounts.append(a)
        for prop, value in [("station", station), ("inputs", amounts), ("output", item(output)),
                            ("output_quantity", count), ("seconds", seconds), ("made_message", unreal.Text(message)),
                            ("sort_order", 120 + order)]:
            recipe.set_editor_property(prop, value)
        save(recipe)
    unreal.log(f"create_hunting: wrote {len(RECIPES)} recipes.")


create_items()
create_recipes()

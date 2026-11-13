"""Sets up more weapons: spears, crossbows and bolts.

- Spears (copper, iron, steel), smithed at an anvil: they reach further than
  a sword (4.2 m) but strike slower, and set against a charging beast they hit
  up to 60% harder (UCombatComponent::BracedHit).
- Crossbow, smithed at an anvil: slow to reload but shoots hard and true;
  it shoots bolts, made at a workbench from an iron bar and a plank.

Each wears out and is mended like other weapons (see create_durability.py).

Run after create_items.py, create_crafting.py and create_hunting.py, headless
with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_weapons.py"

Items and recipes are always written, so edit them here.
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
Cat = unreal.ItemCategory
Skill = unreal.Skill
A, W = unreal.CraftStation.ANVIL, unreal.CraftStation.WORKBENCH


def create(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    return asset_tools.create_asset(name, folder, asset_class, factory)


def item(name):
    found = unreal.load_asset(f"/Game/Items/{name}")
    if found is None:
        raise RuntimeError(f"No item {name}.")
    return found


# Weapons: ID: (name, examine, value, accuracy, strength, seconds, melee reach, shot range, charge bonus,
#               Precision needed, durability, repair bar)
WEAPONS = {
    "CopperSpear": ("Copper spear", "A copper point on a long haft. Keep them at arm's length, and then some.", 30,
                    3, 4, 3.0, 420.0, 0.0, 0.6, 1, 120, "CopperBar"),
    "IronSpear": ("Iron spear", "Plant it and let the boar run onto it.", 100, 8, 10, 3.0, 420.0, 0.0, 0.6, 10, 250, "IronBar"),
    "SteelSpear": ("Steel spear", "A hunter's spear, long and keen.", 340, 15, 18, 3.0, 440.0, 0.0, 0.6, 20, 500, "SteelBar"),
    "Crossbow": ("Crossbow", "Slow to wind, but it hits like a hammer.", 260, 22, 24, 4.2, 0.0, 2600.0, 0.0, 10, 300, "IronBar"),
}

# Recipes: ID: (station, [(input, count)], output, count, message, Smithing level, XP)
RECIPES = {
    "SmithCopperSpear": (A, [("CopperBar", 1), ("Plank", 2)], "CopperSpear", 1, "You hammer out a copper spear.", 4, 55),
    "SmithIronSpear": (A, [("IronBar", 2), ("OakPlank", 1)], "IronSpear", 1, "You hammer out an iron spear.", 11, 110),
    "SmithSteelSpear": (A, [("SteelBar", 2), ("AshPlank", 1)], "SteelSpear", 1, "You hammer out a steel spear.", 27, 190),
    "SmithCrossbow": (A, [("IronBar", 2), ("OakPlank", 2), ("Wool", 1)], "Crossbow", 1, "You put together a crossbow.", 15, 150),
    "MakeBolts": (W, [("IronBar", 1), ("Plank", 1)], "Bolts", 12, "You make twelve crossbow bolts.", 0, 0),
}


def create_items():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
    bolts = create("Bolts", "/Game/Items", unreal.ItemDefinition, factory)
    for prop, value in [("display_name", unreal.Text("Bolts")), ("examine", unreal.Text("Short, heavy and iron-headed.")),
                        ("category", Cat.RESOURCE), ("stackable", True), ("value", 3)]:
        bolts.set_editor_property(prop, value)
    assets.save_loaded_asset(bolts, only_if_is_dirty=False)
    for item_id, (name, examine, value, accuracy, strength, seconds, reach, shot, bonus, precision, uses, bar) in WEAPONS.items():
        weapon = create(item_id, "/Game/Items", unreal.ItemDefinition, factory)
        for prop, setting in [("display_name", unreal.Text(name)), ("examine", unreal.Text(examine)), ("category", Cat.WEAPON),
                              ("stackable", False), ("value", value), ("equip_slot", unreal.EquipSlot.WEAPON),
                              ("accuracy_bonus", accuracy), ("strength_bonus", strength), ("attack_seconds", seconds),
                              ("melee_reach", reach), ("attack_range", shot), ("charge_bonus", bonus),
                              ("ammo", bolts if shot > 0 else None),
                              ("requirements", {Skill.PRECISION: precision} if precision > 1 else {}),
                              ("durability", uses), ("repair_item", item(bar)), ("repair_station", A)]:
            weapon.set_editor_property(prop, setting)
        assets.save_loaded_asset(weapon, only_if_is_dirty=False)


def create_recipes():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.RecipeDefinition)
    for order, (recipe_id, (station, inputs, output, count, message, level, xp)) in enumerate(RECIPES.items()):
        recipe = create(recipe_id, "/Game/Crafting/Recipes", unreal.RecipeDefinition, factory)
        amounts = []
        for input_name, quantity in inputs:
            a = unreal.ItemAmount()
            a.set_editor_property("item", item(input_name))
            a.set_editor_property("quantity", quantity)
            amounts.append(a)
        for prop, setting in [("station", station), ("inputs", amounts), ("output", item(output)), ("output_quantity", count),
                              ("seconds", 3.0), ("made_message", unreal.Text(message)), ("sort_order", 240 + order),
                              ("required_level", max(level, 1)), ("xp", xp)]:
            recipe.set_editor_property(prop, setting)
        if level > 0:  # (No skill is the default, and Python can't name it.)
            recipe.set_editor_property("skill", Skill.SMITHING)
        assets.save_loaded_asset(recipe, only_if_is_dirty=False)
    unreal.log_warning(f"create_weapons: wrote {len(WEAPONS) + 1} items and {len(RECIPES)} recipes.")


create_items()
create_recipes()

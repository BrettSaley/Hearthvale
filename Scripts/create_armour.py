"""Sets up armour: things to wear that soften the blows wild animals land.

- Leather (cap, jerkin, trousers, gloves, boots), stitched at a workbench from hides
- A wooden shield, from planks at a workbench
- Copper, iron and steel helms, chestplates, greaves and shields, smithed at an
  anvil (training Smithing); iron takes Guard 5 to wear, steel Guard 15
- Silversteel, steel worked with silver: the best, and lighter; Guard 30

Shields can also catch a blow outright (BLOCK), and everything worn has a
weight (WEIGHT) that slows the wearer a little (at most 15%): iron is the
heaviest, steel and silversteel lighter.

Each piece's defence adds up (UEquipmentComponent): at 40 defence half of every
blow is turned aside. A shield is no use while a bow is held.

Run after create_items.py, create_crafting.py and create_hunting.py (for
hides), headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_armour.py"

Items and recipes are always written, so edit them here.
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
Cat = unreal.ItemCategory
Slot = unreal.EquipSlot
Skill = unreal.Skill
Station = unreal.CraftStation


def create(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    return asset_tools.create_asset(name, folder, asset_class, factory)


def save(asset):
    assets.save_loaded_asset(asset, only_if_is_dirty=False)


def item(name):
    found = unreal.load_asset(f"/Game/Items/{name}")
    if found is None:
        raise RuntimeError(f"No item {name}.")
    return found


# ID: (name, examine, slot, defence, value, Guard to wear, station, [(input, count)], Smithing level, Smithing XP)
W, A = Station.WORKBENCH, Station.ANVIL
ARMOUR = {
    "LeatherCap": ("Leather cap", "Stiff hide, shaped to a head.", Slot.HEAD, 1, 20, 1, W, [("Hide", 1)], 0, 0),
    "LeatherJerkin": ("Leather jerkin", "Hide, stitched double over the chest.", Slot.BODY, 3, 50, 1, W, [("Hide", 3)], 0, 0),
    "LeatherTrousers": ("Leather trousers", "Tough, if not comfortable.", Slot.LEGS, 2, 35, 1, W, [("Hide", 2)], 0, 0),
    "LeatherGloves": ("Leather gloves", "Saves your knuckles.", Slot.HANDS, 1, 18, 1, W, [("Hide", 1)], 0, 0),
    "LeatherBoots": ("Leather boots", "Good for long walks and kicking boars.", Slot.FEET, 1, 18, 1, W, [("Hide", 1)], 0, 0),
    "WoodenShield": ("Wooden shield", "Three planks and some hope.", Slot.OFF_HAND, 2, 25, 1, W, [("Plank", 3)], 0, 0),

    "CopperHelm": ("Copper helm", "Dents easily. Better it than you.", Slot.HEAD, 2, 30, 1, A, [("CopperBar", 1)], 2, 40),
    "CopperChestplate": ("Copper chestplate", "Shiny, soft and heavy.", Slot.BODY, 5, 75, 1, A, [("CopperBar", 3)], 5, 110),
    "CopperGreaves": ("Copper greaves", "They clank when you walk.", Slot.LEGS, 3, 50, 1, A, [("CopperBar", 2)], 4, 75),
    "CopperShield": ("Copper shield", "Copper over a plank frame.", Slot.OFF_HAND, 3, 45, 1, A, [("CopperBar", 2), ("Plank", 1)], 3, 70),

    "IronHelm": ("Iron helm", "A proper helmet.", Slot.HEAD, 4, 110, 5, A, [("IronBar", 2)], 12, 130),
    "IronChestplate": ("Iron chestplate", "Stops a boar's tusk. Mostly.", Slot.BODY, 9, 230, 5, A, [("IronBar", 4)], 16, 260),
    "IronGreaves": ("Iron greaves", "Solid iron from hip to ankle.", Slot.LEGS, 6, 170, 5, A, [("IronBar", 3)], 14, 200),
    "IronShield": ("Iron shield", "Iron bands on oak.", Slot.OFF_HAND, 6, 160, 5, A, [("IronBar", 3), ("OakPlank", 1)], 13, 190),

    "SteelHelm": ("Steel helm", "Bright, hard and well fitted.", Slot.HEAD, 7, 320, 15, A, [("SteelBar", 2)], 30, 220),
    "SteelChestplate": ("Steel chestplate", "Let the bear try.", Slot.BODY, 15, 650, 15, A, [("SteelBar", 4)], 35, 420),
    "SteelGreaves": ("Steel greaves", "Light for steel, and very strong.", Slot.LEGS, 10, 480, 15, A, [("SteelBar", 3)], 32, 320),
    "SteelShield": ("Steel shield", "Steel on springy ash.", Slot.OFF_HAND, 10, 450, 15, A, [("SteelBar", 3), ("AshPlank", 1)], 31, 300),

    "SilversteelHelm": ("Silversteel helm", "Steel chased with silver. It hardly weighs a thing.", Slot.HEAD, 9, 700, 30, A,
                        [("SteelBar", 1), ("SilverBar", 1)], 40, 300),
    "SilversteelChestplate": ("Silversteel chestplate", "Bright as a lake at noon, and twice as hard.", Slot.BODY, 19, 1400, 30, A,
                              [("SteelBar", 3), ("SilverBar", 2)], 45, 550),
    "SilversteelGreaves": ("Silversteel greaves", "They don't clank. They ring.", Slot.LEGS, 13, 1000, 30, A,
                           [("SteelBar", 2), ("SilverBar", 1)], 42, 420),
    "SilversteelShield": ("Silversteel shield", "Silver-rimmed steel on ash.", Slot.OFF_HAND, 13, 950, 30, A,
                          [("SteelBar", 2), ("SilverBar", 1), ("AshPlank", 1)], 41, 400),
}

# How heavy each piece is to wear, by set and slot; shields' chance of catching a blow outright.
WEIGHT = {"WoodenShield": 3.0}  # (Each leather piece weighs 1.)
for _set, (_helm, _body, _legs, _shield) in {"Copper": (3, 8, 5, 4), "Iron": (4, 10, 7, 5), "Steel": (3, 8, 6, 4),
                                              "Silversteel": (3, 7, 5, 4)}.items():
    WEIGHT.update({f"{_set}Helm": _helm, f"{_set}Chestplate": _body, f"{_set}Greaves": _legs, f"{_set}Shield": _shield})
BLOCK = {"WoodenShield": 0.10, "CopperShield": 0.12, "IronShield": 0.15, "SteelShield": 0.18, "SilversteelShield": 0.22}


def weight_of(item_id):
    return float(WEIGHT.get(item_id, 1.0 if item_id.startswith("Leather") else 0.0))


def create_items():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
    for item_id, (name, examine, slot, defence, value, guard, *_rest) in ARMOUR.items():
        made = create(item_id, "/Game/Items", unreal.ItemDefinition, factory)
        for prop, setting in [("display_name", unreal.Text(name)), ("examine", unreal.Text(examine)),
                              ("category", Cat.ARMOUR), ("stackable", False), ("value", value),
                              ("equip_slot", slot), ("defence_bonus", defence),
                              ("weight", weight_of(item_id)), ("block_chance", BLOCK.get(item_id, 0.0)),
                              ("requirements", {Skill.GUARD: guard} if guard > 1 else {})]:
            made.set_editor_property(prop, setting)
        save(made)


def create_recipes():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.RecipeDefinition)
    for order, (item_id, (name, _examine, _slot, _defence, _value, _guard, station, inputs, level, xp)) in enumerate(ARMOUR.items()):
        verb = "stitch" if inputs[0][0] == "Hide" else "make" if station == W else "hammer out"
        recipe = create(f"Make{item_id}", "/Game/Crafting/Recipes", unreal.RecipeDefinition, factory)
        amounts = []
        for input_name, quantity in inputs:
            a = unreal.ItemAmount()
            a.set_editor_property("item", item(input_name))
            a.set_editor_property("quantity", quantity)
            amounts.append(a)
        for prop, setting in [("station", station), ("inputs", amounts), ("output", item(item_id)), ("output_quantity", 1),
                              ("seconds", 3.0), ("made_message", unreal.Text(f"You {verb} {'some' if name.endswith('s') else 'a'} {name.lower()}.")),
                              ("sort_order", 200 + order), ("required_level", max(level, 1)), ("xp", xp)]:
            recipe.set_editor_property(prop, setting)
        if level > 0:  # (No skill is the default, and Python can't name it.)
            recipe.set_editor_property("skill", Skill.SMITHING)
        save(recipe)
    unreal.log(f"create_armour: wrote {len(ARMOUR)} armour items and recipes.")


create_items()
create_recipes()

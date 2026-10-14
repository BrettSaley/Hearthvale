"""Creates the starting item data assets in /Game/Items.

Run headless (with the editor closed):
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_items.py"
or in the editor: Tools > Execute Python Script.

Items that already exist are left alone, so edits made in the editor are kept.
Cooking data (what raw food cooks into) is set by create_cooking.py.
To rebuild an item from this list, delete its asset first.
"""

import unreal

FOLDER = "/Game/Items"
Cat = unreal.ItemCategory
Tool = unreal.ToolType
Slot = unreal.EquipSlot
Skill = unreal.Skill

# Asset name (the item ID): settings. Names are original to this game.
ITEMS = {
    "Coins": dict(name="Coins", examine="Round, shiny and spendable.", category=Cat.CURRENCY, stackable=True, value=1),

    # Tools: higher power gathers faster; requirements gate who can use them.
    "CopperAxe": dict(name="Copper axe", examine="Soft copper, but it'll fell a tree.", category=Cat.TOOL, value=16,
                      tool=Tool.AXE, power=1),
    "IronAxe": dict(name="Iron axe", examine="A solid iron axe.", category=Cat.TOOL, value=56,
                    tool=Tool.AXE, power=2, requires={Skill.WOODCUTTING: 10}),
    "SteelAxe": dict(name="Steel axe", examine="Keen steel.", category=Cat.TOOL, value=200,
                     tool=Tool.AXE, power=3, requires={Skill.WOODCUTTING: 20}),
    "CopperPickaxe": dict(name="Copper pickaxe", examine="Bends a little on the hard stuff.", category=Cat.TOOL, value=16,
                          tool=Tool.PICKAXE, power=1),
    "IronPickaxe": dict(name="Iron pickaxe", examine="A sturdy iron pickaxe.", category=Cat.TOOL, value=56,
                        tool=Tool.PICKAXE, power=2, requires={Skill.MINING: 10}),
    "SteelPickaxe": dict(name="Steel pickaxe", examine="Bites deep into rock.", category=Cat.TOOL, value=200,
                         tool=Tool.PICKAXE, power=3, requires={Skill.MINING: 20}),
    "FishingNet": dict(name="Fishing net", examine="For scooping fish out of the shallows.", category=Cat.TOOL, value=5,
                       tool=Tool.FISHING_NET, power=1),

    # Weapons
    "CopperSword": dict(name="Copper sword", examine="A starter's blade. Better than fists.", category=Cat.WEAPON, value=26,
                        slot=Slot.WEAPON, accuracy=4, strength=3),
    "IronSword": dict(name="Iron sword", examine="A reliable iron sword.", category=Cat.WEAPON, value=91,
                      slot=Slot.WEAPON, accuracy=10, strength=8, requires={Skill.PRECISION: 10}),
    "SteelSword": dict(name="Steel sword", examine="Light, sharp, deadly.", category=Cat.WEAPON, value=325,
                       slot=Slot.WEAPON, accuracy=18, strength=15, requires={Skill.PRECISION: 20}),

    # Gathered resources
    "Logs": dict(name="Logs", examine="Plain logs from a plain tree.", category=Cat.RESOURCE, value=4),
    "OakLogs": dict(name="Oak logs", examine="Heavy, dense oak.", category=Cat.RESOURCE, value=20),
    "AshLogs": dict(name="Ash logs", examine="Pale, springy ash wood.", category=Cat.RESOURCE, value=45),
    "CopperOre": dict(name="Copper ore", examine="Ore flecked with copper.", category=Cat.RESOURCE, value=5),
    "IronOre": dict(name="Iron ore", examine="Rusty-red iron ore.", category=Cat.RESOURCE, value=17),
    "SilverOre": dict(name="Silver ore", examine="It glints even in the dark.", category=Cat.RESOURCE, value=75),

    # Fish as caught (cook them on a fire; the IDs are just the fish names).
    "Perch": dict(name="Raw perch", examine="A little striped fish. Cook it on a fire.", category=Cat.RESOURCE, value=5),
    "Trout": dict(name="Raw trout", examine="Speckled and slippery. Cook it on a fire.", category=Cat.RESOURCE, value=20),
    "Pike": dict(name="Raw pike", examine="All teeth. Cook it on a fire.", category=Cat.RESOURCE, value=40),
    "CookedMeat": dict(name="Cooked meat", examine="Juicy.", category=Cat.FOOD, value=5, heal=4),

    # Farming: seeds (planted in farm patches; create_skilling.py links them to their crops) and produce.
    "PotatoSeed": dict(name="Potato seed", examine="Plant it and wait. Patience is a skill too.", category=Cat.RESOURCE, stackable=True, value=2),
    "OnionSeed": dict(name="Onion seed", examine="Tiny, black and full of tears.", category=Cat.RESOURCE, stackable=True, value=6),
    "CabbageSeed": dict(name="Cabbage seed", examine="Grows into something leafy.", category=Cat.RESOURCE, stackable=True, value=15),
    "PumpkinSeed": dict(name="Pumpkin seed", examine="A flat seed with big ambitions.", category=Cat.RESOURCE, stackable=True, value=40),
    "Potato": dict(name="Potato", examine="Lumpy and honest.", category=Cat.FOOD, value=4, heal=2),
    "Onion": dict(name="Onion", examine="Layers upon layers.", category=Cat.FOOD, value=10, heal=2),
    "Cabbage": dict(name="Cabbage", examine="A tight green head of leaves.", category=Cat.FOOD, value=22, heal=4),
    "Pumpkin": dict(name="Pumpkin", examine="Heavy, orange and very round.", category=Cat.FOOD, value=55, heal=7),

    # Refined: made at stations from gathered things (see create_crafting.py).
    "Plank": dict(name="Plank", examine="Sawn from plain logs.", category=Cat.RESOURCE, value=8),
    "OakPlank": dict(name="Oak plank", examine="A sturdy oak plank.", category=Cat.RESOURCE, value=35),
    "AshPlank": dict(name="Ash plank", examine="Light and springy.", category=Cat.RESOURCE, value=80),
    "CopperBar": dict(name="Copper bar", examine="Soft, bright metal.", category=Cat.RESOURCE, value=12),
    "IronBar": dict(name="Iron bar", examine="Plain, honest iron.", category=Cat.RESOURCE, value=40),
    "SteelBar": dict(name="Steel bar", examine="Iron, worked until it gave up its weakness.", category=Cat.RESOURCE, value=100),
    "SilverBar": dict(name="Silver bar", examine="Cool and gleaming.", category=Cat.RESOURCE, value=120),
    "GrilledPerch": dict(name="Grilled perch", examine="Crisp skin, flaky inside.", category=Cat.FOOD, value=10, heal=4),
    "GrilledTrout": dict(name="Grilled trout", examine="Smells wonderful.", category=Cat.FOOD, value=35, heal=7),
    "GrilledPike": dict(name="Grilled pike", examine="Mind the bones.", category=Cat.FOOD, value=70, heal=10),
    "BakedPotato": dict(name="Baked potato", examine="Hot all the way through.", category=Cat.FOOD, value=8, heal=4),
    "OnionSoup": dict(name="Onion soup", examine="Sweet and savoury.", category=Cat.FOOD, value=25, heal=6),
    "CabbageStew": dict(name="Cabbage stew", examine="Hearty and green.", category=Cat.FOOD, value=45, heal=9),
    "PumpkinSoup": dict(name="Pumpkin soup", examine="Thick, orange and warming.", category=Cat.FOOD, value=80, heal=12),
    "FishStew": dict(name="Fish stew", examine="Trout, potato and onion. A proper meal.", category=Cat.FOOD, value=70, heal=14),
    "Jade": dict(name="Jade", examine="Polished green stone.", category=Cat.VALUABLE, value=90),
    "Amber": dict(name="Amber", examine="Glowing, honey-coloured.", category=Cat.VALUABLE, value=170),
    "Sapphire": dict(name="Sapphire", examine="A deep blue cut gem.", category=Cat.VALUABLE, value=320),
    "Ruby": dict(name="Ruby", examine="A blazing red cut gem.", category=Cat.VALUABLE, value=850),
    "SilverRing": dict(name="Silver ring", examine="A plain silver band.", category=Cat.VALUABLE, value=150, slot=Slot.RING),
    "JadeRing": dict(name="Jade ring", examine="Silver set with jade.", category=Cat.VALUABLE, value=280, slot=Slot.RING),
    "AmberRing": dict(name="Amber ring", examine="Silver set with amber.", category=Cat.VALUABLE, value=380, slot=Slot.RING),
    "SapphireRing": dict(name="Sapphire ring", examine="Silver set with a sapphire.", category=Cat.VALUABLE, value=560, slot=Slot.RING),
    "RubyRing": dict(name="Ruby ring", examine="Silver set with a ruby. Breathtaking.", category=Cat.VALUABLE, value=1200, slot=Slot.RING),

    # Valuables (for drop tables later)
    "UncutJade": dict(name="Uncut jade", examine="A rough green stone.", category=Cat.VALUABLE, value=40),
    "UncutAmber": dict(name="Uncut amber", examine="Rough amber, warm to the touch.", category=Cat.VALUABLE, value=80),
    "UncutSapphire": dict(name="Uncut sapphire", examine="A rough blue gem.", category=Cat.VALUABLE, value=150),
    "UncutRuby": dict(name="Uncut ruby", examine="A rough red gem. Rare!", category=Cat.VALUABLE, value=400),
}


def set_props(item, data):
    item.set_editor_property("display_name", unreal.Text(data["name"]))
    item.set_editor_property("examine", unreal.Text(data["examine"]))
    item.set_editor_property("category", data["category"])
    item.set_editor_property("stackable", data.get("stackable", False))
    item.set_editor_property("value", data.get("value", 0))
    item.set_editor_property("tool_type", data.get("tool", Tool.NONE))
    item.set_editor_property("tool_power", data.get("power", 0))
    item.set_editor_property("equip_slot", data.get("slot", Slot.NONE))
    item.set_editor_property("accuracy_bonus", data.get("accuracy", 0))
    item.set_editor_property("strength_bonus", data.get("strength", 0))
    item.set_editor_property("defence_bonus", data.get("defence", 0))
    item.set_editor_property("heal_amount", data.get("heal", 0))
    item.set_editor_property("requirements", data.get("requires", {}))


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
    created = 0
    for asset_name, data in ITEMS.items():
        path = f"{FOLDER}/{asset_name}"
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            continue
        item = asset_tools.create_asset(asset_name, FOLDER, unreal.ItemDefinition, factory)
        set_props(item, data)
        unreal.EditorAssetLibrary.save_loaded_asset(item, only_if_is_dirty=False)
        created += 1
    unreal.log(f"create_items: created {created} item(s), {len(ITEMS) - created} already existed.")


main()

"""Sets up alpha trophies and the amulets smithed from them.

Some herds of boars, wolves and bears are led by an alpha (AWildlifeHerd):
bigger, tougher and harder-hitting. Killed, it leaves a trophy, which a smith
can set in silver as an amulet for the neck slot:

- Great tusk (alpha boar) -> tusk charm: a sharper eye
- Alpha fang (alpha wolf) -> fang amulet: a heavier blow
- Bear claw (alpha bear) -> claw talisman: a bit of everything

Run after create_items.py and create_crafting.py, headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_trophies.py"

Always written, so edit them here.
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
Cat = unreal.ItemCategory


def create(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    return asset_tools.create_asset(name, folder, asset_class, factory)


TROPHIES = {
    "GreatTusk": ("Great tusk", "Yellowed, curved and longer than your forearm.", 150),
    "AlphaFang": ("Alpha fang", "From the wolf that led the pack.", 180),
    "BearClaw": ("Bear claw", "One claw. It's the size of a knife.", 300),
}

# Amulet: (name, examine, value, trophy, accuracy, strength, defence, Smithing level, XP)
AMULETS = {
    "TuskCharm": ("Tusk charm", "A boar's tusk on a silver chain. Your aim feels steadier.", 450, "GreatTusk", 4, 0, 1, 20, 160),
    "FangAmulet": ("Fang amulet", "A wolf's fang in silver. Your blows land heavier.", 520, "AlphaFang", 0, 3, 1, 25, 200),
    "ClawTalisman": ("Claw talisman", "A bear's claw bound in silver. You feel unshakeable.", 900, "BearClaw", 2, 3, 3, 30, 260),
}


def main():
    item_factory = unreal.DataAssetFactory()
    item_factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
    made = {}
    for item_id, (name, examine, value) in TROPHIES.items():
        trophy = create(item_id, "/Game/Items", unreal.ItemDefinition, item_factory)
        for prop, setting in [("display_name", unreal.Text(name)), ("examine", unreal.Text(examine)), ("category", Cat.VALUABLE),
                              ("stackable", False), ("value", value)]:
            trophy.set_editor_property(prop, setting)
        assets.save_loaded_asset(trophy, only_if_is_dirty=False)
        made[item_id] = trophy
    recipe_factory = unreal.DataAssetFactory()
    recipe_factory.set_editor_property("data_asset_class", unreal.RecipeDefinition)
    silver = unreal.load_asset("/Game/Items/SilverBar")
    for order, (item_id, (name, examine, value, trophy, accuracy, strength, defence, level, xp)) in enumerate(AMULETS.items()):
        amulet = create(item_id, "/Game/Items", unreal.ItemDefinition, item_factory)
        for prop, setting in [("display_name", unreal.Text(name)), ("examine", unreal.Text(examine)), ("category", Cat.VALUABLE),
                              ("stackable", False), ("value", value), ("equip_slot", unreal.EquipSlot.NECK),
                              ("accuracy_bonus", accuracy), ("strength_bonus", strength), ("defence_bonus", defence)]:
            amulet.set_editor_property(prop, setting)
        assets.save_loaded_asset(amulet, only_if_is_dirty=False)
        recipe = create(f"Smith{item_id}", "/Game/Crafting/Recipes", unreal.RecipeDefinition, recipe_factory)
        inputs = []
        for input_item in (made[trophy], silver):
            a = unreal.ItemAmount()
            a.set_editor_property("item", input_item)
            a.set_editor_property("quantity", 1)
            inputs.append(a)
        for prop, setting in [("station", unreal.CraftStation.ANVIL), ("inputs", inputs), ("output", amulet), ("output_quantity", 1),
                              ("seconds", 3.0), ("made_message", unreal.Text(f"You set the {TROPHIES[trophy][0].lower()} in silver.")),
                              ("sort_order", 270 + order), ("skill", unreal.Skill.SMITHING), ("required_level", level), ("xp", xp)]:
            recipe.set_editor_property(prop, setting)
        assets.save_loaded_asset(recipe, only_if_is_dirty=False)
    unreal.log_warning(f"create_trophies: wrote {len(TROPHIES)} trophies and {len(AMULETS)} amulets.")


main()

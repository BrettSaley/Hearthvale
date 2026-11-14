"""Sets up hammers, for mending what raiders battered.

Build pieces take a battering from raiding wolves (URaidSubsystem). Hold a
hammer and right-click near a battered piece to mend 10 of it, for one of what
it's built from (AHearthvaleCharacter::MendNearby). Hammers wear like any tool
and are mended at the anvil.

Run after create_items.py and create_crafting.py, headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_hammers.py"

Always written, so edit them here.
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary


def create(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    return asset_tools.create_asset(name, folder, asset_class, factory)


def item(name):
    return unreal.load_asset(f"/Game/Items/{name}")


# ID: (name, examine, value, uses, bar, [(input, count)], Smithing level, XP)
HAMMERS = {
    "CopperHammer": ("Copper hammer", "For knocking things back into shape.", 20, 120, "CopperBar", [("CopperBar", 1), ("Plank", 1)], 1, 40),
    "IronHammer": ("Iron hammer", "Heavy-headed and true.", 60, 300, "IronBar", [("IronBar", 1), ("OakPlank", 1)], 8, 90),
}


def main():
    item_factory = unreal.DataAssetFactory()
    item_factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
    recipe_factory = unreal.DataAssetFactory()
    recipe_factory.set_editor_property("data_asset_class", unreal.RecipeDefinition)
    for order, (item_id, (name, examine, value, uses, bar, inputs, level, xp)) in enumerate(HAMMERS.items()):
        hammer = create(item_id, "/Game/Items", unreal.ItemDefinition, item_factory)
        for prop, setting in [("display_name", unreal.Text(name)), ("examine", unreal.Text(examine)), ("category", unreal.ItemCategory.TOOL),
                              ("stackable", False), ("value", value), ("tool_type", unreal.ToolType.HAMMER), ("tool_power", 1),
                              ("durability", uses), ("repair_item", item(bar)), ("repair_station", unreal.CraftStation.ANVIL)]:
            hammer.set_editor_property(prop, setting)
        assets.save_loaded_asset(hammer, only_if_is_dirty=False)
        recipe = create(f"Smith{item_id}", "/Game/Crafting/Recipes", unreal.RecipeDefinition, recipe_factory)
        amounts = []
        for input_name, quantity in inputs:
            a = unreal.ItemAmount()
            a.set_editor_property("item", item(input_name))
            a.set_editor_property("quantity", quantity)
            amounts.append(a)
        for prop, setting in [("station", unreal.CraftStation.ANVIL), ("inputs", amounts), ("output", hammer), ("output_quantity", 1),
                              ("seconds", 3.0), ("made_message", unreal.Text(f"You hammer out a {name.lower()}.")),
                              ("sort_order", 280 + order), ("skill", unreal.Skill.SMITHING), ("required_level", level), ("xp", xp)]:
            recipe.set_editor_property(prop, setting)
        assets.save_loaded_asset(recipe, only_if_is_dirty=False)
    unreal.log_warning(f"create_hammers: wrote {len(HAMMERS)} hammers and their recipes.")


main()

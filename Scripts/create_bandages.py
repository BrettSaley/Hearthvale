"""Sets up bandages, for the bleeding wounds boars, wolves and bears can give.

A bandage (three from a ball of wool at a workbench) stops bleeding and heals a
little; use it like food (right-click it held, or "Bind wounds" in the item
menu). How bleeding works is in UHealthComponent.

Run after create_items.py, create_crafting.py and create_farming.py (for wool),
headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_bandages.py"

Always written, so edit it here.
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary


def create(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    return asset_tools.create_asset(name, folder, asset_class, factory)


def main():
    item_factory = unreal.DataAssetFactory()
    item_factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
    bandage = create("Bandage", "/Game/Items", unreal.ItemDefinition, item_factory)
    for prop, value in [("display_name", unreal.Text("Bandage")), ("examine", unreal.Text("Clean wool, wound tight. For when the wolves get close.")),
                        ("category", unreal.ItemCategory.MISC), ("stackable", True), ("value", 6), ("bandage_heal", 2)]:
        bandage.set_editor_property(prop, value)
    assets.save_loaded_asset(bandage, only_if_is_dirty=False)

    recipe_factory = unreal.DataAssetFactory()
    recipe_factory.set_editor_property("data_asset_class", unreal.RecipeDefinition)
    recipe = create("MakeBandages", "/Game/Crafting/Recipes", unreal.RecipeDefinition, recipe_factory)
    wool = unreal.ItemAmount()
    wool.set_editor_property("item", unreal.load_asset("/Game/Items/Wool"))
    wool.set_editor_property("quantity", 1)
    for prop, value in [("station", unreal.CraftStation.WORKBENCH), ("inputs", [wool]), ("output", bandage), ("output_quantity", 3),
                        ("seconds", 2.4), ("made_message", unreal.Text("You roll three bandages.")), ("sort_order", 260)]:
        recipe.set_editor_property(prop, value)
    assets.save_loaded_asset(recipe, only_if_is_dirty=False)
    unreal.log_warning("create_bandages: wrote the bandage and its recipe.")


main()

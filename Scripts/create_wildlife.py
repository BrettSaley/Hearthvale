"""Makes the colours of the wild animals the game places when it starts.

Foxes, boars, wolves, bears and goats aren't baked into the level (rabbits and
deer are, by create_world.py): AWildlifeHerd::SpawnPlanned places them where
UWorldGenerator::PlanWildlife says, tinted with these materials in
/Game/World/Materials (the basic shape material in plain colours, like
create_world.py's tints). Foxes' tail tips use create_world.py's MI_Shell.

Run after create_world.py, headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_wildlife.py"
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# Dark on purpose, as in create_world.py: auto exposure brightens everything a lot. (r, g, b)
TINTS = {
    "FoxFur": (0.42, 0.12, 0.025),
    "BoarHide": (0.06, 0.04, 0.028), "Tusk": (0.7, 0.65, 0.5),
    "WolfFur": (0.17, 0.17, 0.18), "WolfDark": (0.035, 0.035, 0.04),
    "BearFur": (0.06, 0.035, 0.018), "BearMuzzle": (0.22, 0.14, 0.07),
    "GoatCoat": (0.7, 0.68, 0.62), "Horn": (0.08, 0.065, 0.05),
}


def main():
    parent = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    for name, rgb in TINTS.items():
        path = f"/Game/World/Materials/MI_{name}"
        if assets.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = asset_tools.create_asset(f"MI_{name}", "/Game/World/Materials", unreal.MaterialInstanceConstant,
                                          unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*rgb, 1.0))
        assets.save_loaded_asset(mi, only_if_is_dirty=False)
    unreal.log(f"create_wildlife: wrote {len(TINTS)} tints.")


main()

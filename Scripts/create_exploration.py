"""Content for exploring the island: the lost caches (ALostCache, Source/Hearthvale/RPG/World/LostCache.h).

- /Game/World/Exploration/M_RuinStone: weathered grey stone for the ruins' standing stones
- /Game/World/Exploration/M_CacheWood: dark old wood for the chests left in them

The caches themselves aren't in the level: the game places them where
ALostCache::PlanCaches says when it starts on the island. Without these
materials they're plain grey.

Run any time, headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_exploration.py"
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
FOLDER = "/Game/World/Exploration"

# name: (base colour, roughness)
MATERIALS = {
    "M_RuinStone": ((0.075, 0.075, 0.068), 0.9),
    "M_CacheWood": ((0.09, 0.05, 0.025), 0.75),
}


def create_material(name, rgb, roughness):
    path = f"{FOLDER}/{name}"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    material = asset_tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    color = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    color.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 200)
    rough.set_editor_property("r", roughness)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    assets.save_loaded_asset(material, only_if_is_dirty=False)


for name, (rgb, roughness) in MATERIALS.items():
    create_material(name, rgb, roughness)
unreal.log_warning(f"create_exploration: wrote {len(MATERIALS)} materials.")

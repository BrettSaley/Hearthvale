"""Content for the weather and seasons (UWorldClock, AWeatherEffects in Source/Hearthvale/RPG/World):

- /Game/World/Weather/M_Rain: the rain streaks, unlit and see-through
- /Game/World/Weather/M_Snow: the snowflakes, soft white
- Seasonal catches on the fishing spots: trout running in spring, mackerel and tuna
  shoaling in summer, the salmon run in autumn, char under the winter cold

Without the materials the weather still works, with plain grey drops.

Run after create_gathering.py and create_fishing.py (it adds to their fishing
spots, and they overwrite these settings when run again), headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_weather.py"
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
FOLDER = "/Game/World/Weather"

# Fishing spot: [(season, fish, 1 in)]; each is an extra fish now and then, only in its season.
SEASONAL_CATCHES = {
    "PerchSpot": [(unreal.Season.SPRING, "Trout", 8)],
    "TroutSpot": [(unreal.Season.AUTUMN, "Salmon", 10)],
    "PikeSpot": [(unreal.Season.WINTER, "Char", 12)],
    "CarpSpot": [(unreal.Season.SUMMER, "Carp", 5)],
    "SalmonSpot": [(unreal.Season.AUTUMN, "Salmon", 4)],
    "CharSpot": [(unreal.Season.WINTER, "Char", 4)],
    "MackerelSpot": [(unreal.Season.SUMMER, "Mackerel", 4)],
    "BassSpot": [(unreal.Season.SPRING, "SeaBass", 6)],
    "TunaSpot": [(unreal.Season.SUMMER, "Tuna", 8)],
    "SwordfishSpot": [(unreal.Season.AUTUMN, "Swordfish", 8)],
}


def create_material(name):
    path = f"{FOLDER}/{name}"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    return asset_tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def constant(material, value, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def color(material, rgb, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    return node


def create_materials():
    rain = create_material("M_Rain")
    rain.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    rain.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    rain.set_editor_property("used_with_instanced_static_meshes", True)
    mel.connect_material_property(color(rain, (0.55, 0.6, 0.7), -400, 0), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(constant(rain, 0.28, -400, 200), "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(rain)
    assets.save_loaded_asset(rain, only_if_is_dirty=False)

    snow = create_material("M_Snow")
    snow.set_editor_property("used_with_instanced_static_meshes", True)
    mel.connect_material_property(color(snow, (0.9, 0.92, 0.95), -400, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(constant(snow, 0.9, -400, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(color(snow, (0.08, 0.08, 0.09), -400, 300), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(snow)
    assets.save_loaded_asset(snow, only_if_is_dirty=False)


def create_seasonal_catches():
    count = 0
    for spot, catches in SEASONAL_CATCHES.items():
        node = unreal.load_asset(f"/Game/Gathering/Nodes/{spot}")
        if not node:
            unreal.log_warning(f"create_weather: no fishing spot {spot}; run create_gathering.py and create_fishing.py first.")
            continue
        drops = []
        for season, fish, one_in in catches:
            item = unreal.load_asset(f"/Game/Items/{fish}")
            if not item:
                unreal.log_warning(f"create_weather: no item {fish}.")
                continue
            drop = unreal.BonusDrop()
            drop.set_editor_property("item", item)
            drop.set_editor_property("one_in", one_in)
            drop.set_editor_property("quantity", 1)
            seasonal = unreal.SeasonalDrop()
            seasonal.set_editor_property("season", season)
            seasonal.set_editor_property("drop", drop)
            drops.append(seasonal)
            count += 1
        node.set_editor_property("seasonal_drops", drops)
        assets.save_loaded_asset(node, only_if_is_dirty=False)
    unreal.log(f"create_weather: wrote 2 materials and {count} seasonal catches.")


create_materials()
create_seasonal_catches()

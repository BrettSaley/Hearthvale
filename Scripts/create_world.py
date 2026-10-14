"""Builds the island: /Game/World/Lvl_World, the game's open-world level.

The shape, biomes, ground paint and resource spots all come from
UWorldGenerator (Source/Hearthvale/RPG/World/WorldGenerator.cpp); change the
island there. This script turns them into content:
- Ground-paint layers and a placeholder landscape material (flat colours per
  layer, to be swapped for real textures)
- The level (World Partition): landscape, sea, sun, sky, fog, clouds, player start
- Resource nodes across the biomes
- A temporary camp of crafting stations and farm patches at the start, until
  players can build their own

Run it in the editor (Tools > Execute Python Script), or launch the editor with it:
    UnrealEditor.exe Hearthvale.uproject -ExecCmds="py <path>/Scripts/create_world.py" -QuitAfterWorld
It needs the full editor, not the headless commandlet: the landscape's heights are
turned into terrain on the GPU, so the script waits a few seconds before saving.

To rebuild the island after changing the generator, delete Content/World and
run this again.
"""

import time

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
V = unreal.Vector
LEVEL = "/Game/World/Lvl_World"

# Dark on purpose: auto exposure brightens everything a lot.
LAYER_COLORS = {"Grass": (0.03, 0.055, 0.014), "Forest": (0.022, 0.017, 0.009), "Rock": (0.06, 0.056, 0.05),
                "Sand": (0.2, 0.17, 0.11), "Snow": (0.55, 0.57, 0.6)}


def create(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path), False
    return asset_tools.create_asset(name, folder, asset_class, factory), True


def save(asset):
    assets.save_loaded_asset(asset, only_if_is_dirty=False)


def constant_color(material, rgb, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    return node


def create_landscape_material():
    material, made = create("M_Island", "/Game/World/Materials", unreal.Material, unreal.MaterialFactoryNew())
    if not made:
        return material
    blend = mel.create_material_expression(material, unreal.MaterialExpressionLandscapeLayerBlend, -500, 0)
    layers = []
    for name in LAYER_COLORS:
        entry = unreal.LayerBlendInput()
        entry.set_editor_property("layer_name", name)
        entry.set_editor_property("blend_type", unreal.LandscapeLayerBlendType.LB_WEIGHT_BLEND)
        layers.append(entry)
    blend.set_editor_property("layers", layers)
    for i, (name, rgb) in enumerate(LAYER_COLORS.items()):
        mel.connect_material_expressions(constant_color(material, rgb, -900, i * 150), "", blend, f"Layer {name}")
    # Broad light and dark patches so the ground isn't one flat colour.
    noise = mel.create_material_expression(material, unreal.MaterialExpressionNoise, -500, 400)
    noise.set_editor_property("scale", 0.0015)
    noise.set_editor_property("levels", 4)
    noise.set_editor_property("output_min", 0.7)
    noise.set_editor_property("output_max", 1.15)
    multiply = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, 0)
    mel.connect_material_expressions(blend, "", multiply, "A")
    mel.connect_material_expressions(noise, "", multiply, "B")
    mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -200, 200)
    rough.set_editor_property("r", 0.9)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    save(material)
    return material


def create_water_material():
    material, made = create("M_Water", "/Game/World/Materials", unreal.Material, unreal.MaterialFactoryNew())
    if made:
        mel.connect_material_property(constant_color(material, (0.01, 0.05, 0.08), -400, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
        rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 200)
        rough.set_editor_property("r", 0.05)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(material)
        save(material)
    return material


def create_layer_infos():
    infos = []
    for name in LAYER_COLORS:
        path = f"/Game/World/Layers/LI_{name}"
        info = unreal.load_asset(path) if assets.does_asset_exist(path) else unreal.WorldGenerator.create_layer_info("/Game/World/Layers", name)
        save(info)
        infos.append(info)
    return infos


def spawn(actor_class, location, rotation=unreal.Rotator(0, 0, 0), label=None, folder=None):
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(actor_class, location, rotation)
    if label:
        actor.set_actor_label(label)
    if folder:
        actor.set_folder_path(folder)
    return actor


def add_sky(water_material):
    sun = spawn(unreal.DirectionalLight, V(0, 0, 5000), unreal.Rotator(0, -38, 35), "Sun", "Sky")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sun.light_component.set_editor_property("intensity", 8.0)
    spawn(unreal.SkyAtmosphere, V(0, 0, 0), label="SkyAtmosphere", folder="Sky")
    sky_light = spawn(unreal.SkyLight, V(0, 0, 3000), label="SkyLight", folder="Sky")
    sky_light.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky_light.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.ExponentialHeightFog, V(0, 0, 0), label="Fog", folder="Sky")
    spawn(unreal.VolumetricCloud, V(0, 0, 0), label="Clouds", folder="Sky")
    # The sea (and the lakes, which sit at the same level): a big plane at Z = 0, no collision.
    sea = spawn(unreal.StaticMeshActor, V(0, 0, 0), label="Sea", folder="Sky")
    mesh = sea.static_mesh_component
    mesh.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    mesh.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    mesh.set_material(0, water_material)
    mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    sea.set_actor_scale3d(V(6000, 6000, 1))


def place_resources():
    nodes = {}
    counts = {}
    for spot in unreal.WorldGenerator.plan_resources():
        kind = str(spot.kind)
        if kind not in nodes:
            nodes[kind] = unreal.load_asset(f"/Game/Gathering/Nodes/{kind}")
        node = spawn(unreal.ResourceNode, spot.location, unreal.Rotator(0, 0, spot.yaw))
        node.set_editor_property("definition", nodes[kind])
        node.set_folder_path(f"Resources/{kind}")
        counts[kind] = counts.get(kind, 0) + 1
    unreal.log_warning(f"create_world: resources {counts}")


def place_starter_camp():
    """Stations and patches by the start until players can build their own."""
    def ground(x, y):
        return V(x, y, unreal.WorldGenerator.get_ground_height(x, y))
    for name, x, y, yaw in [("Workbench", 1200, -900, 140), ("Furnace", 1500, 200, 180), ("Anvil", 1200, 1000, 220), ("Range", -300, 1400, 270)]:
        station = spawn(unreal.CraftingStation, ground(x, y), unreal.Rotator(0, 0, yaw), name, "StarterCamp (temporary)")
        station.set_editor_property("definition", unreal.load_asset(f"/Game/Crafting/Stations/{name}"))
    soil = unreal.load_asset("/Game/Farming/Materials/MI_Soil")
    for i, (x, y) in enumerate([(-1300, -500), (-1300, 300)]):
        patch = spawn(unreal.FarmPatch, ground(x, y), unreal.Rotator(0, 0, 90), f"FarmPatch_{i}", "StarterCamp (temporary)")
        patch.set_editor_property("soil_material", soil)


def main():
    if assets.does_asset_exist(LEVEL):
        unreal.log_warning("create_world: the level already exists; delete Content/World to rebuild it.")
        return
    material = create_landscape_material()
    water = create_water_material()
    infos = create_layer_infos()
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level(LEVEL, True):
        unreal.log_error("create_world: couldn't make the level.")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    # A 2 km island stays loaded whole, so distant hills and trees are always there to see.
    unreal.WorldGenerator.disable_streaming(world)
    landscape = unreal.WorldGenerator.build_landscape(world, material, infos)
    unreal.log_warning(f"create_world: landscape {landscape}")
    add_sky(water)
    spawn(unreal.PlayerStart, unreal.WorldGenerator.get_start_location(), label="Start")
    place_resources()
    place_starter_camp()
    save_when_terrain_ready()


def save_when_terrain_ready():
    """Waits for the GPU to finish the landscape, then saves (and quits if asked to)."""
    state = {"start": time.monotonic(), "handle": None}

    def tick(dt):
        if time.monotonic() - state["start"] < 10.0:
            return
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        unreal.log_warning("create_world: done.")
        if "-quitafterworld" in unreal.SystemLibrary.get_command_line().lower():
            unreal.SystemLibrary.quit_editor()

    state["handle"] = unreal.register_slate_post_tick_callback(tick)


main()

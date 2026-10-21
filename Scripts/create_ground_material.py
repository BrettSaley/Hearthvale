"""Imports the ground textures and builds the island's landscape material: /Game/Ground/M_Ground.

The textures are ambientCG surfaces (CC0) in SourceArt/Ground, one per paint
layer: Grass, Forest, Rock, Sand, Snow (see SourceArt/Ground/SOURCES.txt). The
material blends them by the landscape's paint weights. Each colour is sampled
twice, at 3 m tiles up close and 25 m far off, and the two fade into each
other with distance so the repeats don't show; broad light and dark patches
on top break it up further.

create_world.py uses this material when it exists (else flat placeholder
colours). Run headless, then rebuild the level:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_ground_material.py"
Delete /Game/Ground to import and build it again.
"""

import os

import unreal

assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
FOLDER = "/Game/Ground"
SOURCE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Ground")
LAYERS = ["Grass", "Forest", "Rock", "Sand", "Snow"]
# map name: (sRGB colour?, compression)
MAPS = {"Color": (True, unreal.TextureCompressionSettings.TC_DEFAULT),
        "NormalDX": (False, unreal.TextureCompressionSettings.TC_NORMALMAP),
        "Roughness": (False, unreal.TextureCompressionSettings.TC_GRAYSCALE)}
NEAR_TILE, FAR_TILE = 3.0, 25.0  # metres per repeat (landscape quads are 1 m)
FADE_START, FADE_LENGTH = 1500.0, 6000.0  # cm from the camera


def import_textures():
    tasks = []
    for layer in LAYERS:
        for name in MAPS:
            path = os.path.join(SOURCE, layer, f"T_Ground_{layer}_{name}.jpg")
            if assets.does_asset_exist(f"{FOLDER}/{layer}/T_Ground_{layer}_{name}"):
                continue
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", path)
            task.set_editor_property("destination_path", f"{FOLDER}/{layer}")
            task.set_editor_property("automated", True)
            task.set_editor_property("save", False)
            task.set_editor_property("replace_existing", True)
            tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    textures = {}
    for layer in LAYERS:
        for name, (srgb, compression) in MAPS.items():
            texture = unreal.load_asset(f"{FOLDER}/{layer}/T_Ground_{layer}_{name}")
            texture.set_editor_property("srgb", srgb)
            texture.set_editor_property("compression_settings", compression)
            texture.set_editor_property("virtual_texture_streaming", False)
            assets.save_loaded_asset(texture, only_if_is_dirty=False)
            textures[(layer, name)] = texture
    return textures


def node(material, cls, x, y, **props):
    expression = mel.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def blend(material, x, y):
    """A landscape layer blend with one weight-blended input per paint layer."""
    layers = []
    for layer in LAYERS:
        entry = unreal.LayerBlendInput()
        entry.set_editor_property("layer_name", layer)
        entry.set_editor_property("blend_type", unreal.LandscapeLayerBlendType.LB_WEIGHT_BLEND)
        layers.append(entry)
    return node(material, unreal.MaterialExpressionLandscapeLayerBlend, x, y, layers=layers)


def sample(material, texture, coords, x, y, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
    # Shared samplers: 5 layers x 4 samples is more than a material's 16 own samplers.
    s = node(material, unreal.MaterialExpressionTextureSample, x, y, texture=texture,
             sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS,
             sampler_type=sampler)
    mel.connect_material_expressions(coords, "", s, "UVs")
    return s


def build_material(textures):
    path = f"{FOLDER}/M_Ground"
    if assets.does_asset_exist(path):
        unreal.log_warning("create_ground_material: M_Ground already exists; delete it to rebuild.")
        return
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Ground", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    near = node(material, unreal.MaterialExpressionLandscapeLayerCoords, -2200, -400, mapping_scale=NEAR_TILE)
    far = node(material, unreal.MaterialExpressionLandscapeLayerCoords, -2200, -200, mapping_scale=FAR_TILE)

    # 0 up close, 1 far away.
    depth = node(material, unreal.MaterialExpressionPixelDepth, -2200, 200)
    start = node(material, unreal.MaterialExpressionSubtract, -2000, 200, const_b=FADE_START)
    mel.connect_material_expressions(depth, "", start, "A")
    over = node(material, unreal.MaterialExpressionDivide, -1850, 200, const_b=FADE_LENGTH)
    mel.connect_material_expressions(start, "", over, "A")
    fade = node(material, unreal.MaterialExpressionSaturate, -1700, 200)
    mel.connect_material_expressions(over, "", fade, "")

    colors = blend(material, -400, -600)
    normals = blend(material, -400, 0)
    rough = blend(material, -400, 600)
    for i, layer in enumerate(LAYERS):
        y = -900 + i * 380
        color_near = sample(material, textures[(layer, "Color")], near, -1500, y)
        color_far = sample(material, textures[(layer, "Color")], far, -1500, y + 130)
        mix = node(material, unreal.MaterialExpressionLinearInterpolate, -1100, y)
        mel.connect_material_expressions(color_near, "RGB", mix, "A")
        mel.connect_material_expressions(color_far, "RGB", mix, "B")
        mel.connect_material_expressions(fade, "", mix, "Alpha")
        mel.connect_material_expressions(mix, "", colors, f"Layer {layer}")
        normal = sample(material, textures[(layer, "NormalDX")], near, -1100, y + 130, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_expressions(normal, "RGB", normals, f"Layer {layer}")
        # Greyscale, non-sRGB textures must be sampled as such or the material fails to compile.
        roughness = sample(material, textures[(layer, "Roughness")], near, -1100, y + 260, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
        mel.connect_material_expressions(roughness, "R", rough, f"Layer {layer}")

    # Broad light and dark patches across the island, so even the far tiles don't repeat.
    noise = node(material, unreal.MaterialExpressionNoise, -400, -1000, scale=0.0015, levels=4, output_min=0.75, output_max=1.15)
    shade = node(material, unreal.MaterialExpressionMultiply, -150, -600)
    mel.connect_material_expressions(colors, "", shade, "A")
    mel.connect_material_expressions(noise, "", shade, "B")
    mel.connect_material_property(shade, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normals, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    add_grass_output(material)
    mel.recompile_material(material)
    assets.save_loaded_asset(material, only_if_is_dirty=False)
    unreal.log_warning("create_ground_material: built M_Ground")


# --- Grass: plants the engine scatters itself around the camera, by paint layer ---

# layer: [(mesh, density, scale range, (start, end) of the fade-out in m, casts shadow)]
# Density is Unreal's: instances per 10 m x  10 m patch, so 400 is 4 a square metre.
GRASS_LIB = "/Game/PN_GrassLibrary/Meshes/grassMesh"
FLOWERS = "/Game/PN_MeadowFlowers/Meshes/flowerMesh"
GROUND = "/Game/PN_GroundFoliage/Meshes/groundPlantMesh"
GRASS = {
    # Meadows and open Highlands: dense short turf, clumps of medium grass, tall seeding grass and flowers here and there.
    "Grass": [(f"{GRASS_LIB}/grass_01_04_mesh", 120, (0.8, 1.3), (30, 45), False),
              (f"{GRASS_LIB}/grass_01_07_mesh", 120, (0.8, 1.3), (30, 45), False),
              (f"{GRASS_LIB}/grass_01_05_mesh", 80, (0.8, 1.2), (30, 45), False),
              (f"{GRASS_LIB}/grass_02_04_mesh", 50, (0.8, 1.2), (35, 55), False),
              (f"{GRASS_LIB}/grass_02_07_mesh", 50, (0.8, 1.2), (35, 55), False),
              (f"{GRASS_LIB}/grass_05_02_mesh", 15, (0.8, 1.2), (45, 70), False),
              (f"{GRASS_LIB}/grass_09_02_mesh", 15, (0.8, 1.2), (45, 70), False),
              (f"{GRASS_LIB}/lowGrass_04_02_SM", 10, (0.8, 1.3), (40, 60), False),
              (f"{FLOWERS}/flower_05_04", 6, (0.8, 1.2), (40, 60), False),
              (f"{FLOWERS}/flower_04_01", 5, (0.8, 1.2), (40, 60), False),
              (f"{FLOWERS}/flower_12_03", 5, (0.8, 1.2), (40, 60), False),
              (f"{FLOWERS}/flower_06_03", 6, (0.8, 1.2), (35, 50), False),
              (f"{FLOWERS}/flower_10_03", 2, (0.8, 1.1), (50, 75), False)],
    # The forest floor: leaf litter and small plants in the shade, a little grass.
    "Forest": [(f"{GROUND}/ground_02_05", 60, (0.8, 1.4), (30, 45), False),
               (f"{GROUND}/ground_04_04", 40, (0.8, 1.4), (30, 45), False),
               (f"{GROUND}/ground_05_02", 12, (0.8, 1.3), (40, 60), False),
               (f"{GROUND}/ground_06_08", 10, (0.8, 1.3), (40, 60), False),
               (f"{GROUND}/ground_09_05", 10, (0.8, 1.2), (35, 50), False),
               (f"{GROUND}/ground_12_03", 8, (0.8, 1.2), (35, 50), False),
               (f"{GRASS_LIB}/grass_08_04_mesh", 25, (0.8, 1.2), (30, 45), False)],
    # Dunes: thin tall grass.
    "Sand": [(f"{GRASS_LIB}/grass_12_10_mesh", 8, (0.8, 1.3), (40, 60), False),
             (f"{GRASS_LIB}/grass_12_11_mesh", 6, (0.8, 1.3), (40, 60), False)],
    # Tough grass in the cracks of the rock.
    "Rock": [(f"{GRASS_LIB}/grass_10_04_mesh", 6, (0.7, 1.1), (30, 45), False)],
}

def grass_type(layer, varieties):
    """A LandscapeGrassType for one paint layer: each variety a mesh with its density, size and fade distance."""
    name = f"LGT_{layer}"
    path = f"{FOLDER}/GrassTypes/{name}"
    grass = unreal.load_asset(path) if assets.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, f"{FOLDER}/GrassTypes", unreal.LandscapeGrassType, None)
    entries = []
    for mesh_path, density, scale, (start, end), shadow in varieties:
        mesh = unreal.load_asset(mesh_path)
        if not mesh:
            unreal.log_warning(f"create_ground_material: no mesh {mesh_path}")
            continue
        v = unreal.GrassVariety()
        v.set_editor_property("grass_mesh", mesh)
        d = unreal.PerPlatformFloat()
        d.set_editor_property("default", density)
        v.set_editor_property("grass_density", d)
        s, e = unreal.PerPlatformInt(), unreal.PerPlatformInt()
        s.set_editor_property("default", int(start * 100))
        e.set_editor_property("default", int(end * 100))
        v.set_editor_property("start_cull_distance", s)
        v.set_editor_property("end_cull_distance", e)
        v.set_editor_property("scaling", unreal.GrassScaling.UNIFORM)
        v.set_editor_property("scale_x", unreal.FloatInterval(*scale))
        v.set_editor_property("random_rotation", True)
        v.set_editor_property("align_to_surface", True)
        v.set_editor_property("use_grid", False)
        v.set_editor_property("cast_dynamic_shadow", shadow)
        v.set_editor_property("cast_contact_shadow", shadow)
        entries.append(v)
    grass.set_editor_property("grass_varieties", entries)
    assets.save_loaded_asset(grass, only_if_is_dirty=False)
    return grass


def add_grass_output(material):
    """Feeds each paint layer's weight to its grass type, so plants grow where the layer is painted."""
    if not GRASS:
        return
    out = node(material, unreal.MaterialExpressionLandscapeGrassOutput, 200, 900)
    inputs = []
    for layer in GRASS:
        entry = unreal.GrassInput()
        entry.set_editor_property("name", layer)
        entry.set_editor_property("grass_type", grass_type(layer, GRASS[layer]))
        inputs.append(entry)
    out.set_editor_property("grass_types", inputs)
    for i, layer in enumerate(GRASS):
        weight = node(material, unreal.MaterialExpressionLandscapeLayerSample, -200, 900 + i * 120, parameter_name=layer)
        mel.connect_material_expressions(weight, "", out, layer)


build_material(import_textures())

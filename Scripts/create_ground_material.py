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


def sample(material, texture, coords, x, y, normal=False):
    # Shared samplers: 5 layers x 4 samples is more than a material's 16 own samplers.
    s = node(material, unreal.MaterialExpressionTextureSample, x, y, texture=texture,
             sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS,
             sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
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
        normal = sample(material, textures[(layer, "NormalDX")], near, -1100, y + 130, normal=True)
        mel.connect_material_expressions(normal, "RGB", normals, f"Layer {layer}")
        roughness = sample(material, textures[(layer, "Roughness")], near, -1100, y + 260)
        mel.connect_material_expressions(roughness, "R", rough, f"Layer {layer}")

    # Broad light and dark patches across the island, so even the far tiles don't repeat.
    noise = node(material, unreal.MaterialExpressionNoise, -400, -1000, scale=0.0015, levels=4, output_min=0.75, output_max=1.15)
    shade = node(material, unreal.MaterialExpressionMultiply, -150, -600)
    mel.connect_material_expressions(colors, "", shade, "A")
    mel.connect_material_expressions(noise, "", shade, "B")
    mel.connect_material_property(shade, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normals, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    assets.save_loaded_asset(material, only_if_is_dirty=False)
    unreal.log_warning("create_ground_material: built M_Ground")


build_material(import_textures())

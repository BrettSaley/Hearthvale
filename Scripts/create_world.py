"""Builds the island: /Game/World/Lvl_World, the game's open-world level.

The shape, biomes, ground paint and resource spots all come from
UWorldGenerator (Source/Hearthvale/RPG/World/WorldGenerator.cpp); change the
island there. This script turns them into content:
- Ground-paint layers and a placeholder landscape material (flat colours per
  layer, to be swapped for real textures)
- The level (World Partition): landscape, sea, sun, sky, fog, clouds, player start
- Resource nodes across the biomes
- Scenery (grass, flowers, bushes, ferns, boulders, reeds, lily pads, bridges,
  the stone circle and the watchtower ruin...) in one instanced AWorldScenery
  actor
- Life: flocks of birds and butterflies (ABirdFlock), rabbits and deer
  (AWildlifeHerd), fish leaping in every lake (AFishJumps)
- A temporary camp of crafting stations and farm patches at the start, until
  players can build their own

Run it in the editor (Tools > Execute Python Script), or launch the editor with it:
    UnrealEditor.exe Hearthvale.uproject -ExecCmds="py <path>/Scripts/create_world.py" -QuitAfterWorld
It needs the full editor, not the headless commandlet: the landscape's heights are
turned into terrain on the GPU, so the script waits a few seconds before saving.

To rebuild the island after changing the generator, delete Content/World and
the level's actors (Content/__ExternalActors__/World and
Content/__ExternalObjects__/World) and run this again. Scripts/preview_world.py
draws the generator's plan to an image first, without the editor.
"""

import time

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
V = unreal.Vector
LEVEL = "/Game/World/Lvl_World"

# Dark on purpose: auto exposure brightens everything a lot.
LAYER_COLORS = {"Grass": (0.03, 0.055, 0.014), "Forest": (0.02, 0.028, 0.01), "Rock": (0.06, 0.056, 0.05),
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
        mel.connect_material_property(constant_color(material, (0.015, 0.06, 0.1), -400, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
        rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 200)
        # Not too glossy: a mirror at low angles shows the far bank and reads as a dry pit.
        rough.set_editor_property("r", 0.25)
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


def node_part(shape, material, location, scale):
    """A gathering node's mesh part, from the gathering materials."""
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", unreal.load_asset(f"/Engine/BasicShapes/{shape}"))
    p.set_editor_property("material", unreal.load_asset(f"/Game/Gathering/Materials/MI_{material}"))
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(0, 0, 0), V(*scale)))
    return p


# Fuller trees than create_gathering's first placeholders (nodes have 4 parts): clumps of
# leaves rather than one ball, a broad layered oak, ash tall and columnar. (Conifers are scenery only,
# so a cone-shaped tree is never one you can chop.)
TREE_LOOKS = {
    "Tree": [("Cylinder", "Bark", (0, 0, 130), (0.32, 0.32, 2.6)),
             ("Sphere", "Leaves", (0, 0, 340), (2.8, 2.8, 1.9)),
             ("Sphere", "Leaves", (95, 45, 300), (1.8, 1.8, 1.4)),
             ("Sphere", "Leaves", (-85, -55, 380), (1.9, 1.9, 1.5))],
    "OakTree": [("Cylinder", "Bark", (0, 0, 160), (0.6, 0.6, 3.2)),
                ("Sphere", "OakLeaves", (0, 0, 430), (3.8, 3.8, 2.4)),
                ("Sphere", "OakLeaves", (110, 60, 540), (2.4, 2.4, 1.8)),
                ("Sphere", "OakLeaves", (-110, -70, 390), (2.3, 2.3, 1.6))],
    "AshTree": [("Cylinder", "AshBark", (0, 0, 280), (0.28, 0.28, 5.6)),
                ("Sphere", "AshLeaves", (0, 0, 560), (1.7, 1.7, 2.8)),
                ("Sphere", "AshLeaves", (45, -30, 740), (1.3, 1.3, 2.2)),
                ("Sphere", "AshLeaves", (-45, 30, 420), (1.3, 1.3, 2.0))],
}


def restyle_trees():
    for kind, parts in TREE_LOOKS.items():
        definition = unreal.load_asset(f"/Game/Gathering/Nodes/{kind}")
        definition.set_editor_property("parts", [node_part(*p) for p in parts])
        save(definition)


def place_resources():
    nodes = {}
    counts = {}
    for spot in unreal.WorldGenerator.plan_resources():
        kind = str(spot.kind)
        if kind not in nodes:
            nodes[kind] = unreal.load_asset(f"/Game/Gathering/Nodes/{kind}")
        node = spawn(unreal.ResourceNode, spot.location, unreal.Rotator(0, spot.lean, spot.yaw))
        node.set_actor_scale3d(V(spot.scale, spot.scale, spot.scale * spot.stretch))
        node.set_editor_property("definition", nodes[kind])
        node.set_folder_path(f"Resources/{kind}")
        counts[kind] = counts.get(kind, 0) + 1
    unreal.log_warning(f"create_world: resources {counts}")


# --- Scenery and birds ---

# Tints of the engine's basic-shape material, like the gathering nodes'. (r, g, b)
TINTS = {
    "Grass": (0.03, 0.09, 0.015), "Stem": (0.03, 0.08, 0.015), "BushLeaves": (0.025, 0.07, 0.015), "Fern": (0.02, 0.07, 0.014),
    "FlowerRed": (0.7, 0.05, 0.04), "FlowerYellow": (0.85, 0.62, 0.05), "FlowerWhite": (0.85, 0.85, 0.8),
    "FlowerBlue": (0.12, 0.22, 0.75), "FlowerPurple": (0.38, 0.1, 0.5),
    "MushroomCap": (0.6, 0.06, 0.04), "MushroomStem": (0.75, 0.7, 0.6),
    "Reed": (0.25, 0.32, 0.1), "Cattail": (0.2, 0.1, 0.04), "LilyPad": (0.06, 0.25, 0.06), "LilyFlower": (0.9, 0.72, 0.8),
    "Driftwood": (0.5, 0.45, 0.38), "Shell": (0.9, 0.85, 0.75), "Pebble": (0.3, 0.29, 0.27),
    "GullBody": (0.9, 0.9, 0.9), "GullWing": (0.5, 0.52, 0.56), "Crow": (0.02, 0.02, 0.025),
    "SongbirdBody": (0.4, 0.25, 0.12), "SongbirdWing": (0.22, 0.14, 0.07), "HawkBody": (0.32, 0.2, 0.09), "HawkWing": (0.2, 0.12, 0.05),
    "RabbitFur": (0.3, 0.22, 0.15), "DeerHide": (0.32, 0.18, 0.08), "Fish": (0.45, 0.47, 0.5), "Ripple": (0.6, 0.7, 0.75),
    "GrassDry": (0.1, 0.1, 0.03), "TallGrass": (0.055, 0.095, 0.02), "TallGrassSeed": (0.16, 0.13, 0.06),
    "FoxgloveFlower": (0.45, 0.08, 0.3), "Berry": (0.5, 0.02, 0.04), "ShrubLeaves": (0.02, 0.055, 0.015), "Bracken": (0.035, 0.08, 0.015),
    "SpruceNeedles": (0.006, 0.02, 0.012), "PineNeedles": (0.01, 0.03, 0.014), "BirchBark": (0.45, 0.43, 0.4), "BirchLeaves": (0.05, 0.12, 0.03),
}
TINTED = {}
LEAF_TINTS = {'Leaves': (0.03, 0.09, 0.018), 'OakLeaves': (0.02, 0.06, 0.012), 'AshLeaves': (0.045, 0.075, 0.02)}


def create_tints():
    parent = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    for name, rgb in TINTS.items():
        mi, made = create(f"MI_{name}", "/Game/World/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if made:
            mel.set_material_instance_parent(mi, parent)
            mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*rgb, 1.0))
            save(mi)
        TINTED[name] = mi
    # The gathering nodes' bark, leaves and stone, so scenery matches the trees and rocks.
    for name in ["Bark", "Leaves", "Stone"]:
        TINTED[name] = unreal.load_asset(f"/Game/Gathering/Materials/MI_{name}")
    # Deeper greens for the trees than create_gathering first made (it has these now too): the
    # bright ones read as lime under auto exposure.
    for name, rgb in LEAF_TINTS.items():
        mi = unreal.load_asset(f"/Game/Gathering/Materials/MI_{name}")
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*rgb, 1.0))
        save(mi)


SHAPES = {}


def part(shape, material, location, scale, rotation=(0, 0, 0)):
    """Basic shapes are 100 cm across, centred on their origin. rotation is (roll, pitch, yaw)."""
    if shape not in SHAPES:
        SHAPES[shape] = unreal.load_asset(f"/Engine/BasicShapes/{shape}")
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", TINTED[material])
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    return p


def flower(color):
    return [part("Cylinder", "Stem", (0, 0, 15), (0.012, 0.012, 0.3)), part("Sphere", color, (0, 0, 31), (0.08, 0.08, 0.05))]


def log(material, width, length):
    return [part("Cylinder", material, (0, 0, width * 45), (width, width, length), (0, 90, 0)),
            part("Cylinder", material, (length * 15, 0, width * 90), (width * 0.3, width * 0.3, 0.7), (35, 0, 0))]


# kind: (parts, collides, casts shadow, stops drawing beyond (cm), 0 = never[, only the first part collides])
def scenery_kinds():
    return {
        "Grass": ([part("Cone", "Grass", (0, 0, 24), (0.07, 0.07, 0.48)),
                   part("Cone", "Grass", (0, 8, 20), (0.065, 0.065, 0.42), (18, 0, 0)),
                   part("Cone", "Grass", (7, -6, 19), (0.065, 0.065, 0.4), (-14, 14, 0)),
                   part("Cone", "Grass", (-7, -3, 17), (0.06, 0.06, 0.36), (-6, -18, 0))], False, False, 5500),
        "FlowerRed": (flower("FlowerRed"), False, False, 7000),
        "FlowerYellow": (flower("FlowerYellow"), False, False, 7000),
        "FlowerWhite": (flower("FlowerWhite"), False, False, 7000),
        "FlowerBlue": (flower("FlowerBlue"), False, False, 7000),
        "FlowerPurple": (flower("FlowerPurple"), False, False, 7000),
        "Bush": ([part("Sphere", "BushLeaves", (0, 0, 35), (1.2, 1.2, 0.85)),
                  part("Sphere", "Leaves", (35, 20, 55), (0.75, 0.75, 0.6))], False, True, 25000),
        # Fronds are flattened spheres: flat boxes catch the sky and read as white.
        "Fern": ([part("Sphere", "Fern", (0, 0, 10), (0.8, 0.14, 0.05), (0, -15, 0)),
                  part("Sphere", "Fern", (0, 0, 10), (0.8, 0.14, 0.05), (0, 15, 60)),
                  part("Sphere", "Fern", (0, 0, 10), (0.8, 0.14, 0.05), (12, 0, 120))], False, False, 9000),
        "Mushroom": ([part("Cylinder", "MushroomStem", (0, 0, 5), (0.04, 0.04, 0.1)),
                      part("Sphere", "MushroomCap", (0, 0, 11), (0.13, 0.13, 0.07))], False, False, 5000),
        "Boulder": ([part("Sphere", "Stone", (0, 0, 30), (1.0, 0.85, 0.7)),
                     part("Sphere", "Stone", (40, -25, 18), (0.6, 0.55, 0.5))], True, True, 0),
        "Pebble": ([part("Sphere", "Pebble", (0, 0, 3), (0.22, 0.18, 0.1))], False, False, 5000),
        "Shell": ([part("Sphere", "Shell", (0, 0, 1), (0.08, 0.06, 0.03))], False, False, 3000),
        "FallenLog": (log("Bark", 0.4, 3.6), True, True, 30000),
        "Driftwood": (log("Driftwood", 0.22, 2.4), True, True, 20000),
        "Reed": ([part("Cylinder", "Reed", (0, 0, 75), (0.015, 0.015, 1.5)),
                  part("Cylinder", "Cattail", (0, 0, 140), (0.035, 0.035, 0.18))], False, False, 10000),
        "LilyPad": ([part("Cylinder", "LilyPad", (0, 0, 1), (0.55, 0.55, 0.01))], False, False, 15000),
        "LilyFlower": ([part("Sphere", "LilyFlower", (10, 8, 4), (0.14, 0.14, 0.08))], False, False, 15000),
        # Taller plants.
        "GrassDry": ([part("Cone", "GrassDry", (0, 0, 24), (0.07, 0.07, 0.48)),
                      part("Cone", "GrassDry", (0, 8, 20), (0.065, 0.065, 0.42), (18, 0, 0)),
                      part("Cone", "GrassDry", (7, -6, 19), (0.065, 0.065, 0.4), (-14, 14, 0)),
                      part("Cone", "GrassDry", (-7, -3, 17), (0.06, 0.06, 0.36), (-6, -18, 0))], False, False, 5500),
        "TallGrass": ([part("Cone", "TallGrass", (0, 0, 55), (0.045, 0.045, 1.1)),
                       part("Cone", "TallGrass", (0, 12, 48), (0.04, 0.04, 1.0), (14, 0, 0)),
                       part("Cone", "TallGrass", (10, -8, 45), (0.04, 0.04, 0.95), (-10, 14, 0)),
                       part("Cylinder", "TallGrassSeed", (0, 4, 112), (0.025, 0.025, 0.22), (6, 0, 0))], False, False, 7000),
        "Foxglove": ([part("Sphere", "Fern", (0, 0, 4), (0.35, 0.35, 0.06)),
                      part("Cylinder", "Stem", (0, 0, 55), (0.02, 0.02, 1.1)),
                      part("Sphere", "FoxgloveFlower", (4, 0, 72), (0.1, 0.1, 0.13)),
                      part("Sphere", "FoxgloveFlower", (-3, 3, 88), (0.085, 0.085, 0.11)),
                      part("Sphere", "FoxgloveFlower", (2, -2, 103), (0.065, 0.065, 0.09))], False, False, 8000),
        "Bracken": ([part("Cylinder", "Stem", (0, 0, 30), (0.03, 0.03, 0.6)),
                     part("Sphere", "Bracken", (38, 0, 52), (1.1, 0.24, 0.05), (0, 25, 0)),
                     part("Sphere", "Bracken", (0, 38, 56), (1.1, 0.24, 0.05), (0, 25, 90)),
                     part("Sphere", "Bracken", (-38, 0, 50), (1.1, 0.24, 0.05), (0, 25, 180)),
                     part("Sphere", "Bracken", (0, -38, 54), (1.1, 0.24, 0.05), (0, 25, 270))], False, False, 12000),
        "BerryBush": ([part("Sphere", "BushLeaves", (0, 0, 35), (1.0, 1.0, 0.8)),
                       part("Sphere", "Leaves", (30, -20, 55), (0.7, 0.7, 0.6)),
                       part("Sphere", "Berry", (45, 12, 42), (0.08, 0.08, 0.08)),
                       part("Sphere", "Berry", (-22, 40, 52), (0.08, 0.08, 0.08)),
                       part("Sphere", "Berry", (12, -38, 68), (0.08, 0.08, 0.08))], False, True, 20000),
        "Shrub": ([part("Sphere", "ShrubLeaves", (0, 0, 70), (0.9, 0.9, 1.3)),
                   part("Sphere", "ShrubLeaves", (25, 15, 130), (0.7, 0.7, 1.0)),
                   part("Sphere", "BushLeaves", (-20, -20, 160), (0.6, 0.6, 0.8))], False, True, 25000),
        # Forest trees that aren't gathered: only their trunks block (last field).
        "Spruce": ([part("Cylinder", "Bark", (0, 0, 200), (0.35, 0.35, 4.0)),
                    part("Cone", "SpruceNeedles", (0, 0, 500), (3.6, 3.6, 4.0)),
                    part("Cone", "SpruceNeedles", (0, 0, 800), (2.7, 2.7, 3.6)),
                    part("Cone", "SpruceNeedles", (0, 0, 1080), (1.7, 1.7, 3.0)),
                    part("Cone", "SpruceNeedles", (0, 0, 1280), (0.8, 0.8, 1.6))], True, True, 0, True),
        "Birch": ([part("Cylinder", "BirchBark", (0, 0, 375), (0.16, 0.16, 7.5)),
                   part("Sphere", "BirchLeaves", (0, 0, 640), (1.9, 1.9, 2.6)),
                   part("Sphere", "BirchLeaves", (45, 30, 790), (1.3, 1.3, 1.8)),
                   part("Sphere", "BirchLeaves", (-40, -35, 560), (1.2, 1.2, 1.5))], True, True, 0, True),
        "Pine": ([part("Cylinder", "Bark", (0, 0, 450), (0.3, 0.3, 9.0)),
                  part("Cone", "PineNeedles", (0, 0, 780), (2.6, 2.6, 2.2)),
                  part("Cone", "PineNeedles", (0, 0, 930), (1.8, 1.8, 2.2)),
                  part("Cone", "PineNeedles", (0, 0, 1070), (1.0, 1.0, 1.6))], True, True, 0, True),
        "Sapling": ([part("Cylinder", "Bark", (0, 0, 80), (0.07, 0.07, 1.6)),
                     part("Sphere", "Leaves", (0, 0, 180), (0.9, 0.9, 1.0))], False, True, 15000),
        # Landmarks: a stone circle's stones, and a ruined watchtower's wall sections (scaled down where broken).
        "StandingStone": ([part("Cube", "Stone", (0, 0, 160), (0.8, 1.4, 3.2))], True, True, 0),
        "TowerWall": ([part("Cube", "Stone", (0, 0, 300), (0.9, 2.5, 6.0)),
                       part("Cube", "Stone", (0, 0, 615), (1.0, 0.8, 0.3))], True, True, 0),
        # 18 m of planks across a river, level with the banks, with rails and posts down to the bed.
        "Bridge": ([part("Cube", "Bark", (0, 0, 0), (18, 2.4, 0.15)),
                    part("Cube", "Bark", (0, -115, 90), (18, 0.1, 0.1)),
                    part("Cube", "Bark", (0, 115, 90), (18, 0.1, 0.1)),
                    part("Cylinder", "Bark", (-450, -115, -60), (0.18, 0.18, 3.2)),
                    part("Cylinder", "Bark", (-450, 115, -60), (0.18, 0.18, 3.2)),
                    part("Cylinder", "Bark", (450, -115, -60), (0.18, 0.18, 3.2)),
                    part("Cylinder", "Bark", (450, 115, -60), (0.18, 0.18, 3.2))], True, True, 0),
    }


def place_scenery():
    looks = []
    for kind, (parts, collides, shadow, cull, *trunk_only) in scenery_kinds().items():
        look = unreal.SceneryLook()
        look.set_editor_property("only_first_part_collides", bool(trunk_only and trunk_only[0]))
        look.set_editor_property("kind", kind)
        look.set_editor_property("parts", parts)
        look.set_editor_property("collides", collides)
        look.set_editor_property("casts_shadow", shadow)
        look.set_editor_property("cull_distance", cull)
        looks.append(look)
    scenery = spawn(unreal.WorldScenery, V(0, 0, 0), label="Scenery", folder="Scenery")
    scenery.set_editor_property("looks", looks)
    scenery.rebuild()
    unreal.log_warning(f"create_world: scenery {scenery.get_instance_count()} instances")


# kind: (count, loop radius cm, wander cm, speed cm/s, size, wingbeats/s, glide share, height above the ground cm, body, wings)
BIRDS = {
    "Gulls": (7, 3000, 4000, 900, 2.2, 2.2, 0.65, 1500, "GullBody", "GullWing"),
    "Crows": (9, 2000, 4000, 1000, 1.5, 4.0, 0.3, 2000, "Crow", "Crow"),
    "Songbirds": (12, 900, 3000, 1300, 0.8, 8.0, 0.2, 1000, "SongbirdBody", "SongbirdWing"),
    "Hawk": (1, 5000, 3000, 1100, 3.5, 1.6, 0.85, 3500, "HawkBody", "HawkWing"),
}
# Where each flock circles, in metres: gulls round the coast, crows over the Deepwood,
# songbirds over the Meadows (one right by the start), hawks over the Highlands.
FLOCKS = [("Gulls", 860, 140), ("Gulls", -150, 860), ("Gulls", -860, -150), ("Gulls", 160, -860),
          ("Crows", -450, -150), ("Crows", 420, 280), ("Crows", 150, -480),
          ("Songbirds", 60, 40), ("Songbirds", -180, 180), ("Songbirds", 200, -60), ("Songbirds", -100, -220),
          ("Hawk", 250, 650), ("Hawk", -650, 250), ("Hawk", -200, -700), ("Hawk", 600, -380)]


def place_birds():
    for i, (kind, x, y) in enumerate(FLOCKS):
        count, radius, wander, speed, size, flap, glide, height, body, wings = BIRDS[kind]
        x, y = x * 100, y * 100
        z = unreal.WorldGenerator.get_highest_ground(x, y, radius + wander) + height
        flock = spawn(unreal.BirdFlock, V(x, y, z), label=f"{kind}_{i}", folder="Birds")
        for prop, value in [("count", count), ("radius", radius), ("wander", wander), ("speed", speed), ("size", size),
                            ("flap_rate", flap), ("glide_share", glide),
                            ("body_material", TINTED[body]), ("wing_material", TINTED[wings])]:
            flock.set_editor_property(prop, value)
        flock.rebuild()
    place_butterflies()


# Animals on the ground: (kind, animals per herd, roam cm, body tint, tail/antler tint, how many herds)
WILDLIFE = {"Rabbit": (unreal.WildlifeKind.RABBIT, 4, 2000, "RabbitFur", "Shell", 12),
            "Deer": (unreal.WildlifeKind.DEER, 3, 3500, "DeerHide", "Driftwood", 8)}


def place_wildlife():
    """Rabbits in the Meadows (one lot right by the start), deer in the Deepwood, on dry ground."""
    import random
    rng = random.Random(7)
    spots = [(8.0, -18.0)]
    placed = {kind: 0 for kind in WILDLIFE}
    for _ in range(2000):
        if len(spots) > 1 or placed["Rabbit"] > 0:
            x, y = rng.uniform(-650, 650), rng.uniform(-650, 650)
        else:
            x, y = spots[0]
        biome = unreal.WorldGenerator.get_biome(x * 100, y * 100)
        kind = "Rabbit" if biome == unreal.Biome.MEADOWS else "Deer" if biome == unreal.Biome.DEEPWOOD else None
        ground = unreal.WorldGenerator.get_ground_height(x * 100, y * 100)
        if kind is None or placed[kind] >= WILDLIFE[kind][5] or ground < 100:
            continue
        wild_kind, count, roam, body, detail, _ = WILDLIFE[kind]
        herd = spawn(unreal.WildlifeHerd, V(x * 100, y * 100, ground), label=f"{kind}s_{placed[kind]}", folder="Wildlife")
        for prop, value in [("kind", wild_kind), ("count", count), ("roam", roam),
                            ("body_material", TINTED[body]), ("detail_material", TINTED[detail])]:
            herd.set_editor_property(prop, value)
        herd.rebuild()
        placed[kind] += 1
        spots.append((x, y))
    unreal.log_warning(f"create_world: wildlife {placed}")


def place_fish():
    """A fish now and then leaping in every lake and pond."""
    for i, lake in enumerate(unreal.WorldGenerator.get_lakes()):
        fish = spawn(unreal.FishJumps, V(lake.x, lake.y, 0), label=f"FishJumps_{i}", folder="Wildlife/Fish")
        for prop, value in [("radius", lake.z * 0.6), ("fish_material", TINTED["Fish"]), ("ripple_material", TINTED["Ripple"])]:
            fish.set_editor_property(prop, value)


# Butterflies: the flock actor again, tiny and fluttery, low over some of the flower drifts.
BUTTERFLY_WINGS = ["FlowerYellow", "FlowerWhite", "FlowerBlue", "FlowerRed"]


def place_butterflies(most=40, every=120):
    flowers = [s for s in unreal.WorldGenerator.plan_scenery() if str(s.kind).startswith("Flower")]
    for i, spot in enumerate(flowers[::every][:most]):
        loc = spot.transform.translation
        flock = spawn(unreal.BirdFlock, V(loc.x, loc.y, loc.z + 70), label=f"Butterflies_{i}", folder="Birds/Butterflies")
        for prop, value in [("count", 3), ("radius", 150), ("wander", 400), ("speed", 120), ("size", 0.12),
                            ("flap_rate", 9.0), ("glide_share", 0.1),
                            ("body_material", TINTED["Crow"]), ("wing_material", TINTED[BUTTERFLY_WINGS[i % len(BUTTERFLY_WINGS)]])]:
            flock.set_editor_property(prop, value)
        flock.rebuild()


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
    restyle_trees()
    place_resources()
    create_tints()
    place_scenery()
    place_birds()
    place_wildlife()
    place_fish()
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

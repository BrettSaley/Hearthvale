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


VEGETATION = "/Game/Vegetation"  # baked from the Procedural Vegetation Editor's samples by bake_vegetation.py
# Fab packs (added to the project, not in git; see .gitignore).
ROCKS = "/Game/SimpleRocks/3D_Assets/Meshes/Rocks"
SHRUBS = "/Game/GV_FreeShrubsPack/Meshes/Shrubs/Wind"
BEECH = "/Game/EuropeanBeech/Geometry/SimpleWind"
MAXTREE = "/Game/Maxtree/MT_PM_V060"


def mesh_part(mesh, location, scale, rotation=(0, 0, 0), collides=True, visible=True, material=None):
    """A part showing a mesh with its own materials: one of the baked plants by name, any mesh by full path,
    or an invisible collider ("Collider")."""
    p = unreal.NodeMeshPart()
    path = "/Engine/BasicShapes/Cylinder" if mesh == "Collider" else mesh if mesh.startswith("/") else f"{VEGETATION}/{mesh}"
    p.set_editor_property("mesh", unreal.load_asset(path))
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    p.set_editor_property("collides", collides)
    p.set_editor_property("visible", visible)
    if material:
        p.set_editor_property("material", material)
    return p


def tree(mesh, scale, trunk_width, trunk_height):
    """A detailed tree you walk under: the mesh doesn't collide, an invisible cylinder round the trunk does
    (and is what interaction finds). The collider comes first, for looks that only let the first part collide."""
    return [mesh_part("Collider", (0, 0, trunk_height * 50), (trunk_width, trunk_width, trunk_height), visible=False),
            mesh_part(mesh, (0, 0, 0), scale, collides=False)]


# The trees you chop: a young beech (7.6 m as it comes), a broad beech for the oak, an alder for the ash.
# (Conifers are scenery only, so a conifer is never one you can chop.)
TREE_LOOKS = {
    "Tree": tree(f"{BEECH}/SM_EuropeanBeech_Forest_08", (1.25, 1.25, 1.25), 0.35, 3.0),
    "OakTree": tree(f"{BEECH}/SM_EuropeanBeech_Forest_07", (0.65, 0.65, 0.65), 0.55, 3.0),
    "AshTree": tree(f"{MAXTREE}/SM_MT_PM_V60_Alnus_cremastogyne_01_05", (0.75, 0.75, 0.75), 0.35, 4.0),
}


def rock(ore):
    """An ore rock: a real boulder, half sunk as its mesh is centred on its middle (collision from an invisible cylinder, which interaction finds) with two
    chunks of ore showing on top. Mined, the chunks are gone (ore=None)."""
    stone = mesh_part(f"{ROCKS}/SM_SimpleRock_05A", (0, 0, -10), (0.35, 0.35, 0.32), collides=False)
    parts = [mesh_part("Collider", (0, 0, 30), (1.4, 1.4, 0.6), visible=False), stone]
    if ore:
        material = unreal.load_asset(f"/Game/Gathering/Materials/MI_{ore}")
        parts += [mesh_part(f"{ROCKS}/SM_SimpleRock_16A", (20, 10, 52), (0.08, 0.08, 0.07), (0, 0, 30), collides=False, material=material),
                  mesh_part(f"{ROCKS}/SM_SimpleRock_16A", (-30, 25, 42), (0.06, 0.06, 0.05), (0, 0, 120), collides=False, material=material)]
    return parts


# Ore rocks (4 parts, the most a node has), and how they look mined out.
ROCK_LOOKS = {"CopperRock": "Copper", "IronRock": "Iron", "SilverRock": "Silver"}


def restyle_trees():
    for kind, ore in ROCK_LOOKS.items():
        definition = unreal.load_asset(f"/Game/Gathering/Nodes/{kind}")
        definition.set_editor_property("parts", rock(ore))
        definition.set_editor_property("depleted_parts", rock(None))
        save(definition)
    for kind, parts in TREE_LOOKS.items():
        definition = unreal.load_asset(f"/Game/Gathering/Nodes/{kind}")
        definition.set_editor_property("parts", parts)
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
        "Bush": ([mesh_part(f"{SHRUBS}/Shrub_A/GV_Vol7_Shrub_A_full_type1", (0, 0, 0), (0.5, 0.5, 0.5), collides=False)], False, True, 30000),
        # Fronds are flattened spheres: flat boxes catch the sky and read as white.
        # Forest-floor debris: fallen twigs and small branches.
        "Mushroom": ([mesh_part("/Game/PN_coniferBushes_I/Meshes/Props/Larch_GroundTwig_04", (0, 0, 0), (1.2, 1.2, 1.2), collides=False)], False, False, 5000),
        # Boulders: real rocks (SimpleRocks pack, about 4 m as they come), six shapes.
        **{f"Boulder{i + 1}": ([mesh_part(f"{ROCKS}/SM_SimpleRock_{n}", (0, 0, 0), (0.25, 0.25, 0.25))], True, True, 0)
           for i, n in enumerate(["01A", "03A", "05A", "09A", "13A", "16A"])},
        # Outcrops on steep Highland faces: cliff rocks from RockEnv_Pack (8-15 m as they come).
        **{f"Outcrop{i + 1}": ([mesh_part(f"/Game/RockEnv_Pack/Meshes/Cliff_Rocks/SM_Cliff_Rock_{n}", (0, 0, 0), (0.6, 0.6, 0.6))], True, True, 0)
           for i, n in enumerate([1, 2, 5, 8])},
        "Pebble": ([mesh_part(f"{ROCKS}/SM_SimpleRock_16A", (0, 0, 0), (0.05, 0.05, 0.04))], False, False, 5000),
        "Shell": ([part("Sphere", "Shell", (0, 0, 1), (0.08, 0.06, 0.03))], False, False, 3000),
        # Fallen trunks and driftwood: a dead trunk (2 m tall as it comes) laid on its side.
        "FallenLog": ([mesh_part("/Game/Namaqualand/Meshes/General/NN/SM_dead_quiver_trunk_NN", (0, 0, 15), (1.8, 1.8, 1.8), (0, 90, 0))], True, True, 30000),
        "Driftwood": ([mesh_part("/Game/Namaqualand/Meshes/General/NN/SM_dead_quiver_trunk_NN", (0, 0, 10), (1.1, 1.1, 1.1), (0, 90, 0))], True, True, 20000),
        "Reed": ([mesh_part("/Game/PN_GrassLibrary/Meshes/grassMesh/grass_03_06_mesh", (0, 0, 0), (1.0, 1.0, 1.5), collides=False)], False, False, 10000),
        "LilyPad": ([part("Cylinder", "LilyPad", (0, 0, 1), (0.55, 0.55, 0.01))], False, False, 15000),
        "LilyFlower": ([part("Sphere", "LilyFlower", (10, 8, 4), (0.14, 0.14, 0.08))], False, False, 15000),
        # Taller plants.
        "Foxglove": ([mesh_part("/Game/PN_MeadowFlowers/Meshes/flowerMesh/flower_10_04", (0, 0, 0), (1.0, 1.0, 1.0), collides=False)], False, False, 8000),
        "Bracken": ([mesh_part("/Game/PN_GroundFoliage/Meshes/groundPlantMesh/ground_06_08", (0, 0, 0), (1.6, 1.6, 1.6), collides=False)], False, False, 12000),
        "BerryBush": ([mesh_part(f"{SHRUBS}/Shrub_B/GV_Vol7_Shrub_B_full_type1", (0, 0, 0), (0.4, 0.4, 0.4), collides=False)], False, True, 30000),
        "Shrub": ([mesh_part(f"{SHRUBS}/Shrub_D/GV_Vol7_Shrub_D_full_type1", (0, 0, 0), (0.4, 0.4, 0.45), collides=False)], False, True, 40000),
        # Forest trees that aren't gathered, from the baked plants: only their (invisible) trunk colliders block.
        # The Deepwood is a beech forest: tall forest beeches (31 m as they come).
        "Spruce": (tree(f"{BEECH}/SM_EuropeanBeech_Forest_04", (0.5, 0.5, 0.5), 0.45, 4.0), True, True, 0, True),
        "Birch": (tree(f"{MAXTREE}/SM_MT_PM_V60_Alnus_cremastogyne_01_06", (0.7, 0.7, 0.7), 0.3, 4.0), True, True, 0, True),
        "Pine": (tree("SM_PV_Conifer", (0.5, 0.5, 0.42), 0.3, 4.0), True, True, 0, True),
        "Sapling": ([mesh_part(f"{BEECH}/SM_EuropeanBeech_Sapling_01", (0, 0, 0), (0.8, 0.8, 0.8), collides=False)], False, True, 20000),
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
    # The textured ground from create_ground_material.py if it's been built, else flat placeholder colours.
    material = unreal.load_asset("/Game/Ground/M_Ground") if assets.does_asset_exist("/Game/Ground/M_Ground") else create_landscape_material()
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

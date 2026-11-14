"""Falling stars (AFallenStar, Source/Hearthvale/RPG/World/FallenStar.h): on clear nights a star can fall far
off; mine the glowing rock (Mining 30, a pickaxe) for starmetal ore before it's gone.

- /Game/Items/StarmetalOre: rare, valuable ore
- /Game/Gathering/Nodes/FallenStar: the rock's definition (not placed: the game drops one when a star falls)
- /Game/World/Starfall/M_StarGlow (glowing violet, the streak and the rock's cracks), M_StarRock (scorched dark stone)

Run after create_items.py and create_gathering.py, headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_starfall.py"
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
V = unreal.Vector
FOLDER = "/Game/World/Starfall"
SHAPES = {s: unreal.load_asset(f"/Engine/BasicShapes/{s}") for s in ["Sphere", "Cube", "Cone"]}

# name: (base colour, emissive, roughness)
MATERIALS = {
    "M_StarGlow": ((0.4, 0.25, 0.8), (6.0, 3.5, 14.0), 0.4),
    "M_StarRock": ((0.03, 0.025, 0.035), (0.0, 0.0, 0.0), 0.85),
}


def create(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    return asset_tools.create_asset(name, folder, asset_class, factory)


def create_materials():
    for name, (base, glow, roughness) in MATERIALS.items():
        path = f"{FOLDER}/{name}"
        if assets.does_asset_exist(path):
            assets.delete_asset(path)
        material = asset_tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
        colour = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
        colour.set_editor_property("constant", unreal.LinearColor(*base, 1.0))
        mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
        rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 150)
        rough.set_editor_property("r", roughness)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        if any(glow):
            emissive = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 300)
            emissive.set_editor_property("constant", unreal.LinearColor(*glow, 1.0))
            mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(material)
        assets.save_loaded_asset(material, only_if_is_dirty=False)


def part(shape, material, location, scale, rotation=(0, 0, 0)):
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", unreal.load_asset(f"{FOLDER}/{material}"))
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    return p


def create_ore():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
    ore = create("StarmetalOre", "/Game/Items", unreal.ItemDefinition, factory)
    for prop, value in [("display_name", unreal.Text("Starmetal ore")),
                        ("examine", unreal.Text("Heavy, dark ore from a fallen star. It's faintly warm.")),
                        ("category", unreal.ItemCategory.RESOURCE), ("stackable", True), ("value", 300)]:
        ore.set_editor_property(prop, value)
    assets.save_loaded_asset(ore, only_if_is_dirty=False)
    return ore


def create_node(ore):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GatheringNodeDefinition)
    node = create("FallenStar", "/Game/Gathering/Nodes", unreal.GatheringNodeDefinition, factory)
    # A scorched rock half sunk in its crater, glowing cracks across it.
    parts = [part("Sphere", "M_StarRock", (0, 0, 35), (1.5, 1.3, 1.0)),
             part("Sphere", "M_StarRock", (40, 25, 25), (0.8, 0.7, 0.6)),
             part("Cube", "M_StarGlow", (0, 0, 70), (1.0, 0.06, 0.06), (0, 30, 15)),
             part("Cube", "M_StarGlow", (15, 10, 60), (0.06, 0.9, 0.06), (0, 20, 0))]
    crater = [part("Sphere", "M_StarRock", (0, 0, 5), (2.2, 2.2, 0.15))]
    for prop, value in [("display_name", unreal.Text("Fallen star")), ("action_verb", unreal.Text("Mine")),
                        ("start_message", unreal.Text("You chip at the hot, glowing rock.")),
                        ("success_message", unreal.Text("You prise out some {0}.")), ("skill", unreal.Skill.MINING),
                        ("required_level", 30), ("tool", unreal.ToolType.PICKAXE), ("item", ore), ("xp", 120),
                        ("attempt_interval", 2.6), ("difficulty", 0.15), ("deplete_chance", 0.34), ("respawn_seconds", 100000.0),
                        ("parts", parts), ("depleted_parts", crater)]:
        node.set_editor_property(prop, value)
    assets.save_loaded_asset(node, only_if_is_dirty=False)


create_materials()
create_node(create_ore())
unreal.log_warning("create_starfall: wrote the starmetal ore, the fallen star's rock and 2 materials.")

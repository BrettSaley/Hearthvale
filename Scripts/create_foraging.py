"""Wild things to forage (AForageNode, Source/Hearthvale/RPG/World/ForageNode.h), picked by hand for Farming XP:

- Wild berries from bushes in the Meadows, summer and autumn
- Morels in the Deepwood in spring; chanterelles there in autumn
- Frost lichen on the Highlands rocks, only in winter (worth a good deal at market)
- Glowcaps, rare and far out in the deep woods, spring and autumn (worth more)

Writes the items (/Game/Items), the node definitions (/Game/Gathering/Nodes,
with the seasons they grow in) and a few colours (/Game/World/Foraging).
The nodes aren't placed in the level: the game places them where
AForageNode::PlanForage says when it starts on the island.

Run after create_items.py and create_gathering.py, headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_foraging.py"
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
V = unreal.Vector
S = unreal.Season
FOLDER = "/Game/World/Foraging"
SHAPES = {s: unreal.load_asset(f"/Engine/BasicShapes/{s}") for s in ["Cylinder", "Sphere", "Cone", "Cube"]}

# Item: (name, examine, value, heals)
ITEMS = {
    "WildBerries": ("Wild berries", "A handful of sweet, dark berries.", 6, 2),
    "Morel": ("Morel", "A honeycombed spring mushroom. Delicious.", 18, 3),
    "Chanterelle": ("Chanterelle", "A golden autumn mushroom that smells of apricots.", 16, 3),
    "FrostLichen": ("Frost lichen", "Pale lichen scraped from winter rocks. Healers pay well for it.", 45, 0),
    "Glowcap": ("Glowcap", "A mushroom that glows faintly blue. Rare, and prized.", 160, 0),
}

# Our own colours: name -> (base colour, emissive)
COLOURS = {
    "M_Morel": ((0.12, 0.08, 0.04), (0, 0, 0)),
    "M_Chanterelle": ((0.75, 0.45, 0.05), (0, 0, 0)),
    "M_Lichen": ((0.45, 0.5, 0.4), (0, 0, 0)),
    "M_Glowcap": ((0.1, 0.35, 0.5), (0.2, 1.2, 2.0)),
}


def colour(name):
    return unreal.load_asset(f"{FOLDER}/{name}")


def scenery(name):
    return unreal.load_asset(f"/Game/World/Materials/{name}")


def part(shape, material, location, scale, rotation=(0, 0, 0)):
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", material)
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    return p


def mushrooms(cap, stem, spots, size):
    parts = []
    for x, y, s in spots:
        h = 14 * s * size
        parts.append(part("Cylinder", stem, (x, y, h / 2), (0.06 * s * size, 0.06 * s * size, h / 100)))
        parts.append(part("Sphere", cap, (x, y, h), (0.18 * s * size, 0.18 * s * size, 0.1 * s * size)))
    return parts


def looks():
    leaves, berry, stem, pebble = scenery("MI_ShrubLeaves"), scenery("MI_Berry"), scenery("MI_MushroomStem"), scenery("MI_Pebble")
    bush = [part("Sphere", leaves, (0, 0, 40), (1.0, 1.0, 0.8)), part("Sphere", leaves, (30, 20, 30), (0.6, 0.6, 0.5))]
    bush += [part("Sphere", berry, (x, y, z), (0.07, 0.07, 0.07)) for x, y, z in
             ((40, 10, 50), (-30, 35, 45), (10, -45, 55), (-20, -30, 65), (35, -20, 35), (-45, 0, 40))]
    morels = mushrooms(colour("M_Morel"), stem, ((0, 0, 1.0), (14, 8, 0.8), (-10, 12, 0.7)), 1.3)
    chanterelles = mushrooms(colour("M_Chanterelle"), colour("M_Chanterelle"), ((0, 0, 1.0), (12, -9, 0.8), (-12, 6, 0.9), (4, 14, 0.7)), 1.1)
    lichen = [part("Sphere", pebble, (0, 0, 15), (0.8, 0.6, 0.35))]
    lichen += [part("Sphere", colour("M_Lichen"), (x, y, 26), (0.28, 0.24, 0.06)) for x, y in ((10, 5), (-15, -8), (0, -18))]
    glowcaps = mushrooms(colour("M_Glowcap"), stem, ((0, 0, 1.2), (12, 10, 0.9), (-14, 4, 0.8)), 1.2)
    return {"bush": bush, "morels": morels, "chanterelles": chanterelles, "lichen": lichen, "glowcaps": glowcaps}


# Node: (name, verb, start, level, item, xp, difficulty, deplete chance, respawn s, look, seasons, [(rare, 1 in)])
NODES = {
    "WildBerryBush": ("Berry bush", "Pick", "You pick through the bush for ripe berries.", 1, "WildBerries", 8, -0.3, 0.3, 300.0,
                      "bush", [S.SUMMER, S.AUTUMN], []),
    "MorelPatch": ("Morels", "Gather", "You crouch and cut the morels at the stem.", 10, "Morel", 18, -0.15, 0.5, 600.0,
                   "morels", [S.SPRING], []),
    "ChanterellePatch": ("Chanterelles", "Gather", "You gather the golden mushrooms.", 15, "Chanterelle", 22, -0.15, 0.5, 600.0,
                         "chanterelles", [S.AUTUMN], []),
    "FrostLichen": ("Frost lichen", "Scrape", "You scrape the lichen off the frozen rock.", 25, "FrostLichen", 40, -0.05, 0.6, 900.0,
                    "lichen", [S.WINTER], []),
    "Glowcap": ("Glowcaps", "Gather", "You ease the glowing mushrooms out of the moss.", 35, "Glowcap", 90, 0.0, 1.0, 1800.0,
                "glowcaps", [S.SPRING, S.AUTUMN], []),
}


def create_colours():
    for name, (base, glow) in COLOURS.items():
        path = f"{FOLDER}/{name}"
        if assets.does_asset_exist(path):
            assets.delete_asset(path)
        material = asset_tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
        node = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
        node.set_editor_property("constant", unreal.LinearColor(*base, 1.0))
        mel.connect_material_property(node, "", unreal.MaterialProperty.MP_BASE_COLOR)
        if any(glow):
            emissive = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 200)
            emissive.set_editor_property("constant", unreal.LinearColor(*glow, 1.0))
            mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(material)
        assets.save_loaded_asset(material, only_if_is_dirty=False)


def create(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    return asset_tools.create_asset(name, folder, asset_class, factory)


def create_items():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
    for item_id, (name, examine, value, heals) in ITEMS.items():
        item = create(item_id, "/Game/Items", unreal.ItemDefinition, factory)
        for prop, v in [("display_name", unreal.Text(name)), ("examine", unreal.Text(examine)),
                        ("category", unreal.ItemCategory.FOOD if heals else unreal.ItemCategory.RESOURCE),
                        ("stackable", True), ("value", value), ("heal_amount", heals)]:
            item.set_editor_property(prop, v)
        assets.save_loaded_asset(item, only_if_is_dirty=False)


def create_nodes():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GatheringNodeDefinition)
    parts = looks()
    for kind, (name, verb, start, level, item, xp, difficulty, deplete, respawn, look, seasons, _) in NODES.items():
        node = create(kind, "/Game/Gathering/Nodes", unreal.GatheringNodeDefinition, factory)
        for prop, v in [("display_name", unreal.Text(name)), ("action_verb", unreal.Text(verb)), ("start_message", unreal.Text(start)),
                        ("success_message", unreal.Text("You get some {0}.")), ("skill", unreal.Skill.FARMING),
                        ("required_level", level), ("tool", unreal.ToolType.NONE), ("item", unreal.load_asset(f"/Game/Items/{item}")),
                        ("xp", xp), ("attempt_interval", 2.0), ("difficulty", difficulty), ("deplete_chance", deplete),
                        ("respawn_seconds", respawn), ("blocks_movement", False), ("parts", parts[look]), ("depleted_parts", []),
                        ("seasons", seasons)]:
            node.set_editor_property(prop, v)
        assets.save_loaded_asset(node, only_if_is_dirty=False)
    unreal.log_warning(f"create_foraging: wrote {len(ITEMS)} items, {len(NODES)} wild plants and {len(COLOURS)} colours.")


create_colours()
create_items()
create_nodes()

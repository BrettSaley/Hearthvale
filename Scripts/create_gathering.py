"""Creates the gathering content and places nodes in the starting level.

- The Interact input (E) and the IMC_Hearthvale mapping context
- Placeholder materials (tinted engine basic-shape material)
- Gathering node definitions in /Game/Gathering/Nodes
- A grove, a mine and a fishing pond in /Game/ThirdPerson/Lvl_ThirdPerson

Run headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_gathering.py"

Existing assets are left alone (delete one to rebuild it from here), and nodes
are only placed if the level has none yet.
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
V = unreal.Vector


def create(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path), False
    return asset_tools.create_asset(name, folder, asset_class, factory), True


def save(asset):
    assets.save_loaded_asset(asset, only_if_is_dirty=False)


# --- Input: E to interact ---

def create_input():
    action, made = create("IA_Interact", "/Game/Input/Actions", unreal.InputAction, unreal.InputAction_Factory())
    if made:
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        save(action)
    context, made = create("IMC_Hearthvale", "/Game/Input", unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    if made:
        key = unreal.Key()
        key.set_editor_property("key_name", "E")
        context.map_key(action, key)
        gamepad = unreal.Key()
        gamepad.set_editor_property("key_name", "Gamepad_FaceButton_Left")
        context.map_key(action, gamepad)
        save(context)


# --- Placeholder materials ---

COLORS = {
    "Bark": (0.18, 0.11, 0.06), "Leaves": (0.03, 0.09, 0.018), "OakLeaves": (0.02, 0.06, 0.012),
    "AshBark": (0.55, 0.5, 0.42), "AshLeaves": (0.045, 0.075, 0.02), "Stump": (0.45, 0.32, 0.18),
    "Stone": (0.22, 0.21, 0.2), "Copper": (0.75, 0.35, 0.12), "Iron": (0.4, 0.17, 0.12),
    "Silver": (0.85, 0.88, 0.95), "Water": (0.05, 0.25, 0.5), "Foam": (0.85, 0.92, 1.0),
}
MATERIALS = {}


def create_materials():
    parent = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    for name, (r, g, b) in COLORS.items():
        mi, made = create(f"MI_{name}", "/Game/Gathering/Materials", unreal.MaterialInstanceConstant,
                          unreal.MaterialInstanceConstantFactoryNew())
        if made:
            unreal.MaterialEditingLibrary.set_material_instance_parent(mi, parent)
            unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(r, g, b, 1.0))
            save(mi)
        MATERIALS[name] = mi


# The node parts below need the materials, so make them first.
create_input()
create_materials()


# --- Node definitions ---

SHAPES = {s: unreal.load_asset(f"/Engine/BasicShapes/{s}") for s in ["Cylinder", "Sphere", "Cone", "Cube"]}


def part(shape, material, location, scale, rotation=(0, 0, 0)):
    """Basic shapes are 100 cm across, centred on their origin."""
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", MATERIALS[material])
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    return p


def tree(bark, leaves, trunk_height, trunk_width, canopy):
    return [part("Cylinder", bark, (0, 0, trunk_height / 2), (trunk_width, trunk_width, trunk_height / 100))] + canopy


STUMP = [part("Cylinder", "Stump", (0, 0, 20), (0.45, 0.45, 0.4))]


def rock(ore):
    return [part("Sphere", "Stone", (0, 0, 45), (1.6, 1.4, 1.1)),
            part("Sphere", "Stone", (55, -30, 30), (0.8, 0.8, 0.7)),
            part("Sphere", ore, (-35, 45, 75), (0.32, 0.32, 0.32)),
            part("Sphere", ore, (50, 35, 55), (0.28, 0.28, 0.28))]


MINED = [part("Sphere", "Stone", (0, 0, 35), (1.4, 1.2, 0.8)),
         part("Sphere", "Stone", (55, -30, 25), (0.7, 0.7, 0.6))]
POOL = [part("Cylinder", "Water", (0, 0, 2), (2.2, 2.2, 0.04)),
        part("Cylinder", "Foam", (0, 0, 3), (0.9, 0.9, 0.03))]

Skill, Tool = unreal.Skill, unreal.ToolType
# The trees' parts here are first placeholders: create_world.py restyles them (TREE_LOOKS).
NODES = {
    "Tree": dict(name="Tree", verb="Chop down", start="You swing your axe at the tree.", skill=Skill.WOODCUTTING,
                 level=1, tool=Tool.AXE, item="Logs", xp=25, interval=2.4, difficulty=0.0, deplete=1.0, respawn=8.4,
                 parts=tree("Bark", "Leaves", 260, 0.35, [part("Sphere", "Leaves", (0, 0, 330), (2.2, 2.2, 2.0))]),
                 depleted=STUMP),
    "OakTree": dict(name="Oak", verb="Chop down", start="You swing your axe at the oak.", skill=Skill.WOODCUTTING,
                    level=15, tool=Tool.AXE, item="OakLogs", xp=40, interval=2.4, difficulty=0.05, deplete=0.15, respawn=12,
                    parts=tree("Bark", "OakLeaves", 280, 0.55, [part("Sphere", "OakLeaves", (0, 0, 370), (3.2, 3.2, 2.4)),
                                                              part("Sphere", "OakLeaves", (90, 40, 450), (1.8, 1.8, 1.5))]),
                    depleted=STUMP),
    "AshTree": dict(name="Ash tree", verb="Chop down", start="You swing your axe at the ash tree.", skill=Skill.WOODCUTTING,
                    level=30, tool=Tool.AXE, item="AshLogs", xp=70, interval=2.4, difficulty=0.1, deplete=0.12, respawn=21,
                    parts=tree("AshBark", "AshLeaves", 360, 0.3, [part("Cone", "AshLeaves", (0, 0, 380), (2.2, 2.2, 2.6)),
                                                                part("Cone", "AshLeaves", (0, 0, 520), (1.6, 1.6, 2.0))]),
                    depleted=STUMP),
    "CopperRock": dict(name="Copper rock", verb="Mine", start="You swing your pickaxe at the rock.", skill=Skill.MINING,
                       level=1, tool=Tool.PICKAXE, item="CopperOre", xp=18, interval=2.4, difficulty=0.0, deplete=1.0,
                       respawn=2.4, parts=rock("Copper"), depleted=MINED),
    "IronRock": dict(name="Iron rock", verb="Mine", start="You swing your pickaxe at the rock.", skill=Skill.MINING,
                     level=15, tool=Tool.PICKAXE, item="IronOre", xp=35, interval=2.4, difficulty=0.05, deplete=1.0,
                     respawn=5.4, parts=rock("Iron"), depleted=MINED),
    "SilverRock": dict(name="Silver rock", verb="Mine", start="You swing your pickaxe at the rock.", skill=Skill.MINING,
                       level=30, tool=Tool.PICKAXE, item="SilverOre", xp=55, interval=2.4, difficulty=0.1, deplete=1.0,
                       respawn=30, parts=rock("Silver"), depleted=MINED),
    "PerchSpot": dict(name="Fishing spot", verb="Net", start="You cast out your net.", skill=Skill.FISHING,
                      level=1, tool=Tool.FISHING_NET, item="Perch", xp=15, interval=3.0, difficulty=0.0, deplete=0.0,
                      respawn=0, parts=POOL, depleted=[]),
    "TroutSpot": dict(name="Fishing spot", verb="Net", start="You cast out your net.", skill=Skill.FISHING,
                      level=20, tool=Tool.FISHING_NET, item="Trout", xp=50, interval=3.0, difficulty=0.0, deplete=0.0,
                      respawn=0, parts=POOL, depleted=[]),
    "PikeSpot": dict(name="Fishing spot", verb="Net", start="You cast out your net.", skill=Skill.FISHING,
                     level=30, tool=Tool.FISHING_NET, item="Pike", xp=70, interval=3.0, difficulty=0.05, deplete=0.0,
                     respawn=0, parts=POOL, depleted=[]),
}
DEFINITIONS = {}


def create_nodes():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GatheringNodeDefinition)
    for asset_name, d in NODES.items():
        node, made = create(asset_name, "/Game/Gathering/Nodes", unreal.GatheringNodeDefinition, factory)
        DEFINITIONS[asset_name] = node
        if not made:
            continue
        for prop, value in [("display_name", unreal.Text(d["name"])), ("action_verb", unreal.Text(d["verb"])),
                            ("start_message", unreal.Text(d["start"])), ("skill", d["skill"]),
                            ("required_level", d["level"]), ("tool", d["tool"]),
                            ("item", unreal.load_asset(f"/Game/Items/{d['item']}")), ("xp", d["xp"]),
                            ("attempt_interval", d["interval"]), ("difficulty", d["difficulty"]),
                            ("deplete_chance", d["deplete"]), ("respawn_seconds", d["respawn"]),
                            ("parts", d["parts"]), ("depleted_parts", d["depleted"])]:
            node.set_editor_property(prop, value)
        save(node)


# --- Placing nodes in the level ---

LEVEL = "/Game/ThirdPerson/Lvl_ThirdPerson"
# (definition, x, y). The template arena's corners hold blocks and ramps, but each
# quarter has a clear area: a grove (x<0, y<0), a mine (x<0, y>0) and a fishing
# pond (x>0, y<0). The x>0, y>0 quarter is free for a town later. Easier nodes
# sit nearest the middle.
LAYOUT = [
    ("Tree", -650, -650), ("Tree", -650, -950), ("Tree", -950, -650), ("Tree", -950, -1000),
    ("OakTree", -1300, -750), ("OakTree", -1250, -1080),
    ("AshTree", -850, -1300),
    ("CopperRock", -650, 650), ("CopperRock", -650, 950), ("CopperRock", -950, 650),
    ("IronRock", -950, 1000), ("IronRock", -1300, 750),
    ("SilverRock", -850, 1300),
    ("PerchSpot", 650, -650), ("PerchSpot", 650, -950),
    ("TroutSpot", 950, -700),
    ("PikeSpot", 900, -1300),
]


def place_nodes():
    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    if any(isinstance(a, unreal.ResourceNode) for a in actors):
        unreal.log_warning("create_gathering: level already has resource nodes; not placing more.")
        return
    # Avoid the template's blocks and ramps (anything mid-sized; skip the floor, walls and sky).
    obstacles = []
    for a in actors:
        origin, extent = a.get_actor_bounds(False)
        if 20 < extent.x < 1500 and 20 < extent.y < 1500 and extent.z > 20:
            obstacles.append((origin, extent))
    def clear(x, y, taken):
        inside = abs(x) < 1700 and abs(y) < 1700
        off_blocks = all(abs(x - o.x) > e.x + 100 or abs(y - o.y) > e.y + 100 for o, e in obstacles)
        apart = all((x - tx) ** 2 + (y - ty) ** 2 > 250 ** 2 for tx, ty in taken)
        return inside and off_blocks and apart

    # Each node goes at its spot or, if that's blocked, the nearest clear one.
    offsets = [(0, 0)] + [(r * dx, r * dy) for r in range(100, 900, 100)
                          for dx, dy in [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)]]
    taken = []
    placed = skipped = 0
    for name, x, y in LAYOUT:
        spot = next(((x + dx, y + dy) for dx, dy in offsets if clear(x + dx, y + dy, taken)), None)
        if spot is None:
            skipped += 1
            continue
        x, y = spot
        taken.append(spot)
        node = actor_subsystem.spawn_actor_from_class(unreal.ResourceNode, V(x, y, 0), unreal.Rotator(0, 0, (x * 7 + y * 13) % 360))
        node.set_editor_property("definition", DEFINITIONS[name])
        node.set_actor_label(f"{name}_{placed}")
        node.set_folder_path("Gathering")
        placed += 1
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log_warning(f"create_gathering: placed {placed} nodes, skipped {skipped} blocked spots.")


create_nodes()
place_nodes()

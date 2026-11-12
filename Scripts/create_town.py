"""Sets up the town: people to trade with.

- Odile the trader (/Game/Town/Traders/Odile, a UTraderDefinition): sells seeds,
  a bucket, bait, copper tools and grown, tame livestock (a chicken, a sheep, a pig
  and a cow) for coins, and buys anything worth something for half its value
- Her look and her market stall (a counter with produce on it under a striped
  awning), in plain colours from /Game/Town/Materials
- One placed in Lvl_World near where you start, facing it
- The town notice board (/Game/Town/RequestBoard, a URequestBoardDefinition):
  three requests up at a time from the pool below, each paying coins and XP;
  bigger jobs go up once you've done a few. One placed beside Odile's stall

Run after create_livestock.py (she sells cows, pigs and buckets), headless with the
editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_town.py"

Traders and the board are always rewritten from the tables below, so edit them
here. Each is placed only if the level has none yet.
"""

import math
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
V = unreal.Vector
LEVEL = "/Game/World/Lvl_World"


def create(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path), False
    return asset_tools.create_asset(name, folder, asset_class, factory), True


def save(asset):
    assets.save_loaded_asset(asset, only_if_is_dirty=False)


def item(name):
    found = unreal.load_asset(f"/Game/Items/{name}")
    if found is None:
        raise RuntimeError(f"No item {name}; run the earlier create_*.py scripts first.")
    return found


def animal(name):
    found = unreal.load_asset(f"/Game/Farming/Animals/{name}")
    if found is None:
        raise RuntimeError(f"No animal {name}; run create_farming.py and create_livestock.py first.")
    return found


# --- Colours: dark on purpose, like the other scripts' (auto exposure brightens them a lot) ---

SHAPES = {name: unreal.load_asset(f"/Engine/BasicShapes/{name}") for name in ["Cube", "Sphere", "Cylinder", "Cone"]}
COLORS = {"Skin": (0.55, 0.36, 0.26), "Hair": (0.12, 0.06, 0.03), "Shirt": (0.12, 0.2, 0.32), "Apron": (0.62, 0.58, 0.48),
          "Trousers": (0.1, 0.08, 0.07), "Boots": (0.05, 0.035, 0.025), "AwningRed": (0.45, 0.06, 0.04),
          "AwningCream": (0.7, 0.64, 0.5)}
MATERIALS = {"Face": unreal.load_asset("/Game/Farming/Materials/MI_AnimalFace"),
             "Wood": unreal.load_asset("/Game/Crafting/Materials/MI_Wood"),
             "Bark": unreal.load_asset("/Game/Gathering/Materials/MI_Bark")}
for crop in ["Cabbage", "Pumpkin", "Potato", "Onion"]:
    MATERIALS[crop] = unreal.load_asset(f"/Game/Farming/Materials/MI_{crop}")


def create_materials():
    parent = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    for name, color in COLORS.items():
        mi, _ = create(f"MI_Town{name}", "/Game/Town/Materials", unreal.MaterialInstanceConstant,
                       unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color, 1.0))
        save(mi)
        MATERIALS[name] = mi


def part(shape, material, location, scale, rotation=(0, 0, 0), collides=False):
    """Basic shapes are 100 cm across, centred on their origin. Rotation is (roll, pitch, yaw)."""
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", MATERIALS[material])
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    p.set_editor_property("collides", collides)
    return p


# --- Looks: facing +X, feet at the origin ---

def person():
    return ([part("Cylinder", "Trousers", (0, y, 42), (0.13, 0.13, 0.84)) for y in (-10, 10)]
            + [part("Cube", "Boots", (4, y, 5), (0.26, 0.13, 0.1)) for y in (-10, 10)]
            + [part("Cylinder", "Shirt", (0, 0, 112), (0.4, 0.3, 0.6)),
               part("Cube", "Apron", (14, 0, 96), (0.04, 0.34, 0.66))]
            + [part("Cylinder", "Shirt", (0, y, 114), (0.1, 0.1, 0.5), (y * 0.3, 0, 0)) for y in (-25, 25)]
            + [part("Sphere", "Skin", (0, y, 86), (0.1, 0.1, 0.1)) for y in (-28, 28)]
            + [part("Cylinder", "Skin", (0, 0, 144), (0.11, 0.11, 0.1)),
               part("Sphere", "Skin", (0, 0, 162), (0.3, 0.27, 0.32)),
               part("Sphere", "Hair", (-4, 0, 168), (0.31, 0.29, 0.27)),
               part("Sphere", "Hair", (-14, 0, 150), (0.14, 0.2, 0.22)),
               part("Sphere", "Skin", (15, 0, 159), (0.05, 0.05, 0.06))]
            + [part("Cylinder", "Face", (14, y, 164), (0.03, 0.03, 0.01), (90, 0, 0)) for y in (-6, 6)])


def stall():
    stripes = []
    for i in range(6):
        y = -100 + i * 40
        stripes.append(part("Cube", "AwningRed" if i % 2 == 0 else "AwningCream", (30, y, 238), (1.9, 0.4, 0.03), (0, 8, 0)))
    produce = ([part("Sphere", "Cabbage", (78, y, 118), (0.22, 0.22, 0.2)) for y in (-85, -62)]
               + [part("Sphere", "Pumpkin", (78, y, 120), (0.3, 0.3, 0.24)) for y in (60, 88)]
               + [part("Sphere", "Potato", (70 + dx, -20 + dy, 110), (0.09, 0.07, 0.06)) for dx, dy in ((0, 0), (10, 8), (-6, 12), (6, -10))]
               + [part("Sphere", "Onion", (82 + dx, 20 + dy, 111), (0.08, 0.08, 0.09)) for dx, dy in ((0, 0), (-9, 6), (5, 11))])
    return ([part("Cube", "Wood", (75, 0, 50), (0.6, 2.2, 1.0), collides=True),
             part("Cube", "Wood", (75, 0, 102), (0.72, 2.32, 0.06))]
            + [part("Cylinder", "Bark", (x, y, 125), (0.09, 0.09, 2.5), collides=True) for x in (110, -55) for y in (-118, 118)]
            + stripes + produce
            + [part("Cube", "Wood", (-30, y, 22), (0.45, 0.45, 0.44), (0, 0, 12), collides=True) for y in (-80, 85)])


# --- Traders: ID: (name, title, greetings, call-outs, stock [(item or "@animal", price or 0)], markup) ---

TRADERS = {
    "Odile": ("Odile", "Trader",
              ["What'll it be? I buy most things, at a fair price.",
               "Back again! Seeds are fresh in.",
               "Coin for wool, coin for eggs. Coin for most things, really."],
              ["Seeds and stock! Hens, sheep, pigs, cows!",
               "Got anything to sell? Come and see me.",
               "Fine day for the market!"],
              [("PotatoSeed", 0), ("OnionSeed", 0), ("CabbageSeed", 0), ("PumpkinSeed", 0), ("Bucket", 0), ("Bait", 0),
               ("CopperAxe", 0), ("CopperPickaxe", 0), ("CopperSickle", 0),
               ("@Chicken", 40), ("@Sheep", 120), ("@Pig", 160), ("@Cow", 260)],
              1.5),
}


def create_traders():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.TraderDefinition)
    made = {}
    for trader_id, (name, title, greetings, callouts, stock, markup) in TRADERS.items():
        trader, _ = create(trader_id, "/Game/Town/Traders", unreal.TraderDefinition, factory)
        lines = []
        for what, price in stock:
            line = unreal.TraderStock()
            if what.startswith("@"):
                line.set_editor_property("animal", animal(what[1:]))
            else:
                line.set_editor_property("item", item(what))
            line.set_editor_property("price", price)
            lines.append(line)
        for prop, value in [("display_name", unreal.Text(name)), ("title", unreal.Text(title)),
                            ("greetings", [unreal.Text(t) for t in greetings]),
                            ("call_outs", [unreal.Text(t) for t in callouts]), ("stock", lines), ("markup", markup),
                            ("currency", item("Coins")), ("parts", person()), ("stall_parts", stall())]:
            trader.set_editor_property(prop, value)
        save(trader)
        made[trader_id] = trader
    return made


# --- The notice board: (asker, words, item, how many, coins, skill, XP, requests done first) ---

S = unreal.Skill
REQUESTS = [
    ("Odile the trader", "My hens have gone off laying. Could you spare some eggs?", "Egg", 6, 45, S.FARMING, 30, 0),
    ("Bryn the weaver", "My loom's run dry. Any wool going spare?", "Wool", 4, 70, S.FARMING, 40, 0),
    ("Ivo the carpenter", "I've a barn to raise and not enough timber.", "Logs", 10, 45, S.WOODCUTTING, 60, 0),
    ("Greta the smith", "The forge is hungry. Bring me copper ore.", "CopperOre", 8, 45, S.MINING, 60, 0),
    ("Tam the cook", "Fish supper tonight, if someone'll catch the fish.", "Perch", 5, 30, S.FISHING, 50, 0),
    ("Hal the baker", "Milk for the morning bread, please. Fresh, mind.", "Milk", 3, 40, S.FARMING, 30, 0),
    ("Tam the cook", "A sack of potatoes. The stew pot's empty.", "Potato", 12, 40, S.FARMING, 40, 0),
    ("Ivo the carpenter", "Planks, sawn true. I'll pay well for them.", "Plank", 10, 90, S.WOODCUTTING, 80, 1),
    ("Rook the hunter", "The tannery's short of hides. Can you hunt?", "Hide", 3, 70, S.PRECISION, 50, 1),
    ("Tam the cook", "Three trout for the inn's table.", "Trout", 3, 90, S.FISHING, 90, 1),
    ("Greta the smith", "Iron bars for a new plough, if you've the furnace for it.", "IronBar", 4, 220, S.SMITHING, 150, 2),
    ("Bryn the weaver", "Two wheels of cheese for the harvest feast.", "Cheese", 2, 160, S.FARMING, 100, 2),
    ("Hal the baker", "Butter for the pastry. Churned, not bought.", "Butter", 2, 80, S.FARMING, 60, 2),
    ("Odile the trader", "Pumpkins for the autumn fair. Big ones.", "Pumpkin", 3, 130, S.FARMING, 90, 2),
    ("Odile the trader", "A rich customer wants a truffle. Find me one?", "Truffle", 1, 160, S.FARMING, 120, 3),
    ("Ivo the carpenter", "Oak for the meeting hall's beams.", "OakLogs", 6, 180, S.WOODCUTTING, 150, 3),
]


def board():
    notes = [part("Cube", "AwningCream", (6, y, z), (0.01, 0.3, 0.36), (r, 0, 0)) for y, z, r in ((-45, 150, -4), (0, 135, 3), (45, 152, 6))]
    pins = [part("Sphere", "AwningRed", (8, y, z + 14), (0.03, 0.03, 0.03)) for y, z, _ in ((-45, 150, 0), (0, 135, 0), (45, 152, 0))]
    return ([part("Cylinder", "Bark", (0, y, 105), (0.1, 0.1, 2.1), collides=True) for y in (-78, 78)]
            + [part("Cube", "Wood", (0, 0, 140), (0.08, 1.6, 0.95), collides=True),
               part("Cube", "Wood", (-4, 0, 210), (0.45, 1.85, 0.05), (0, -12, 0))]
            + notes + pins)


def create_board():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.RequestBoardDefinition)
    made, _ = create("RequestBoard", "/Game/Town", unreal.RequestBoardDefinition, factory)
    requests = []
    for asker, text, what, count, coins, skill, xp, done in REQUESTS:
        r = unreal.TownRequest()
        for prop, value in [("asker", unreal.Text(asker)), ("text", unreal.Text(text)), ("item", item(what)),
                            ("quantity", count), ("coins", coins), ("skill", skill), ("xp", xp), ("min_done", done)]:
            r.set_editor_property(prop, value)
        requests.append(r)
    for prop, value in [("display_name", unreal.Text("Notice board")), ("requests", requests), ("spots", 3),
                        ("repost_seconds", 150.0), ("currency", item("Coins")), ("parts", board())]:
        made.set_editor_property(prop, value)
    save(made)
    return made


# --- Odile's stall, a little way from where you start, facing it ---

def place(traders):
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()
    if any(isinstance(a, unreal.Trader) for a in actors):
        unreal.log_warning("create_town: the level already has a trader; not placing another.")
        return
    starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
    start = starts[0].get_actor_location() if starts else V(0, 0, 0)
    height = unreal.WorldGenerator.get_ground_height
    # Walk round a ring until there's flat, dry ground.
    for step in range(36):
        angle = math.radians(35 + step * 10)
        x, y = start.x + 900 * math.cos(angle), start.y + 900 * math.sin(angle)
        heights = [height(x + dx, y + dy) for dx in (-200, 0, 200) for dy in (-200, 0, 200)]
        if min(heights) > 120 and max(heights) - min(heights) < 60:
            break
    yaw = math.degrees(math.atan2(start.y - y, start.x - x))
    actor = subsystem.spawn_actor_from_class(unreal.Trader, V(x, y, min(heights)), unreal.Rotator(0, 0, yaw))
    actor.set_editor_property("definition", traders["Odile"])
    actor.set_actor_label("Trader_Odile")
    actor.set_folder_path("Town")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(f"create_town: Odile's stall at ({x:.0f}, {y:.0f}), facing {yaw:.0f}.")


def place_board(notice_board):
    """Beside the first trader's stall, facing the same way."""
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()
    if any(isinstance(a, unreal.RequestBoard) for a in actors):
        unreal.log_warning("create_town: the level already has a notice board; not placing another.")
        return
    traders = [a for a in actors if isinstance(a, unreal.Trader)]
    if not traders:
        unreal.log_warning("create_town: no trader to put the notice board beside.")
        return
    stall = traders[0]
    spot = stall.get_actor_location() + stall.get_actor_right_vector() * 340.0 + stall.get_actor_forward_vector() * 60.0
    spot.z = unreal.WorldGenerator.get_ground_height(spot.x, spot.y)
    actor = subsystem.spawn_actor_from_class(unreal.RequestBoard, spot, stall.get_actor_rotation())
    actor.set_editor_property("definition", notice_board)
    actor.set_actor_label("NoticeBoard")
    actor.set_folder_path("Town")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(f"create_town: notice board at ({spot.x:.0f}, {spot.y:.0f}).")


create_materials()
place(create_traders())
place_board(create_board())

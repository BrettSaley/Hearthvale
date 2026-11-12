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
- Settlers (/Game/Town/Settlers) and the settlement that lists them
  (/Game/Town/Settlement): people who move into a house you build (a bed under
  a roof that isn't yours) and work for their keep: a woodcutter, a miner, a
  fisher and a herder
- Villagers (/Game/Town/Villagers): Bryn, Ivo, Greta, Tam and Hal, who stroll
  between a few spots round the stall by day and go home at night. Talk to them
  each day, or give them things they like, to make friends: friends give presents.
  Placed in Lvl_World (each only if they aren't there yet)

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
          "AwningCream": (0.7, 0.64, 0.5), "ShirtGreen": (0.12, 0.24, 0.1), "ShirtRed": (0.35, 0.08, 0.06),
          "ShirtBrown": (0.25, 0.15, 0.08), "HairBlond": (0.5, 0.36, 0.12), "HairGrey": (0.32, 0.31, 0.3)}
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

def person(shirt="Shirt", hair="Hair", apron="Apron"):
    return ([part("Cylinder", "Trousers", (0, y, 42), (0.13, 0.13, 0.84)) for y in (-10, 10)]
            + [part("Cube", "Boots", (4, y, 5), (0.26, 0.13, 0.1)) for y in (-10, 10)]
            + [part("Cylinder", shirt, (0, 0, 112), (0.4, 0.3, 0.6))]
            + ([part("Cube", apron, (14, 0, 96), (0.04, 0.34, 0.66))] if apron else [])
            + [part("Cylinder", shirt, (0, y, 114), (0.1, 0.1, 0.5), (y * 0.3, 0, 0)) for y in (-25, 25)]
            + [part("Sphere", "Skin", (0, y, 86), (0.1, 0.1, 0.1)) for y in (-28, 28)]
            + [part("Cylinder", "Skin", (0, 0, 144), (0.11, 0.11, 0.1)),
               part("Sphere", "Skin", (0, 0, 162), (0.3, 0.27, 0.32)),
               part("Sphere", hair, (-4, 0, 168), (0.31, 0.29, 0.27)),
               part("Sphere", hair, (-14, 0, 150), (0.14, 0.2, 0.22)),
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


# --- Settlers: ID: (name, title, arrival, lines, job, produce, how many, work seconds, shirt, hair) ---

GATHERER = unreal.SettlerJob.GATHERER
HERDER = unreal.SettlerJob.HERDER
SETTLERS = {
    "Wren": ("Wren", "Woodcutter", "A roof and a bed! I'll keep you in logs.",
             ["Good wood round here.", "Mind your fingers near the axe.", "Smell that? Fresh-cut pine."],
             GATHERER, "Logs", 2, 60.0, "ShirtGreen", "HairBlond"),
    "Dace": ("Dace", "Miner", "Somewhere dry to sleep at last. I'll dig you some ore.",
             ["There's copper in those hills.", "Dark work, mining. I like it.", "My back's not what it was."],
             GATHERER, "CopperOre", 2, 90.0, "ShirtBrown", "HairGrey"),
    "Marlo": ("Marlo", "Fisher", "A house by the water? I'll fill your chest with fish.",
              ["They're biting today.", "Nothing like a quiet morning on the water.", "Caught one this long. Honest."],
              GATHERER, "Perch", 2, 80.0, "Shirt", "Hair"),
    "Sorrel": ("Sorrel", "Herder", "I'll mind your animals: wool, milk and eggs, into the chest.",
               ["Happy animals give more, you know.", "The sheep have names. Don't ask.", "Keep the trough full, would you?"],
               HERDER, None, 1, 30.0, "ShirtRed", "Hair"),
}


def create_settlers():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.SettlerDefinition)
    made = []
    for settler_id, (name, title, arrival, lines, job, produce, count, work, shirt, hair) in SETTLERS.items():
        settler, _ = create(settler_id, "/Game/Town/Settlers", unreal.SettlerDefinition, factory)
        for prop, value in [("display_name", unreal.Text(name)), ("title", unreal.Text(title)),
                            ("arrival", unreal.Text(arrival)), ("lines", [unreal.Text(t) for t in lines]),
                            ("job", job), ("produce", item(produce) if produce else None), ("produce_quantity", count),
                            ("work_seconds", work), ("meal_seconds", 480.0), ("parts", person(shirt, hair, None))]:
            settler.set_editor_property(prop, value)
        save(settler)
        made.append(settler)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.SettlementDefinition)
    settlement, _ = create("Settlement", "/Game/Town", unreal.SettlementDefinition, factory)
    for prop, value in [("settlers", made), ("arrival_seconds", 120.0), ("home_range", 1200.0)]:
        settlement.set_editor_property(prop, value)
    save(settlement)


# --- Villagers: ID: (title, stranger, friend, close, night, wet, likes, (present, how many), shirt, hair, apron) ---

VILLAGERS = {
    "Bryn": ("Weaver", ["Morning. You're new here, aren't you?", "Mind the loom threads if you come by."],
             ["Good wool makes good cloth. You've been a help.", "Fancy a scarf? Bring me wool and I'll see."],
             ["Honestly, the town's better for having you.", "I wove your name into the hall banner. Don't tell anyone."],
             "Off home. The loom can wait till morning.", "Wet wool smells awful. Truly.",
             ["Wool", "Cheese"], ("Wool", 3), "ShirtRed", "HairBlond", "Apron"),
    "Ivo": ("Carpenter", ["Hm. Need something built?", "Measure twice, cut once."],
            ["You've a good eye for a beam.", "That hut of yours is coming on."],
            ["Take these planks. I cut them too long for my job anyway.", "If you ever build a hall, I'm your man."],
            "Too dark to saw straight. Night.", "Rain warps the timber. I hate rain.",
            ["OakLogs", "Plank"], ("Plank", 4), "ShirtBrown", "HairGrey", None),
    "Greta": ("Smith", ["Stand back from the forge.", "Iron doesn't work itself."],
              ["You've got the arms for smithing, you know.", "Bring me good ore and I'll remember it."],
              ["You're the only one who doesn't flinch at the sparks.", "Here, I kept the best bar back for you."],
              "Fire's banked for the night.", "Rain on a hot anvil. What a hiss.",
              ["IronBar", "CopperOre"], ("IronBar", 1), "Shirt", "Hair", "Apron"),
    "Tam": ("Cook", ["Hungry? Everyone's hungry.", "The stew's not ready yet."],
            ["You bring the fish, I'll bring the pot.", "Try the chowder. Go on."],
            ["You eat at my table any time, friend.", "Saved you a bowl. Don't tell Hal."],
            "Kitchen's closed. Come back for breakfast.", "Good weather for soup, this.",
            ["Trout", "Truffle", "Onion"], ("FishChowder", 1), "ShirtGreen", "Hair", "Apron"),
    "Hal": ("Baker", ["Up before dawn, me. Every day.", "Bread's a tuppence. Or it was."],
            ["Fresh milk makes the best bread.", "You're a good sort. Have a roll."],
            ["I'd bake you a cake if I had the eggs. Hint.", "The whole town's talking about you. Nicely."],
            "Bed now. The ovens start at four.", "Damp gets into the flour. Ruins it.",
            ["Milk", "Butter", "Egg"], ("ButteredPotato", 2), "Shirt", "HairBlond", "Apron"),
}


def create_villagers():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.VillagerDefinition)
    made = {}
    for villager_id, (title, stranger, friend, close, night, wet, likes, present, shirt, hair, apron) in VILLAGERS.items():
        villager, _ = create(villager_id, "/Game/Town/Villagers", unreal.VillagerDefinition, factory)
        for prop, value in [("display_name", unreal.Text(villager_id)), ("title", unreal.Text(title)),
                            ("stranger_lines", [unreal.Text(t) for t in stranger]),
                            ("friend_lines", [unreal.Text(t) for t in friend]),
                            ("close_lines", [unreal.Text(t) for t in close]),
                            ("night_line", unreal.Text(night)), ("wet_line", unreal.Text(wet)),
                            ("likes", [item(i) for i in likes]), ("present", item(present[0])),
                            ("present_quantity", present[1]), ("parts", person(shirt, hair, apron))]:
            villager.set_editor_property(prop, value)
        save(villager)
        made[villager_id] = villager
    return made


def place_villagers(villagers):
    """Round the square in front of the first trader's stall: a home out on the edge, and a few spots near the middle."""
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()
    traders = [a for a in actors if isinstance(a, unreal.Trader)]
    if not traders:
        unreal.log_warning("create_town: no trader to put the villagers round.")
        return
    here = {a.get_editor_property("definition").get_name() for a in actors
            if isinstance(a, unreal.Villager) and a.get_editor_property("definition")}
    stall = traders[0]
    square = stall.get_actor_location() + stall.get_actor_forward_vector() * 600.0
    height = unreal.WorldGenerator.get_ground_height

    def dry(angle_deg, distance):
        # The first dry spot out along a bearing, coming in towards the square.
        for step in range(8):
            d = distance * (1.0 - step * 0.1)
            x = square.x + d * math.cos(math.radians(angle_deg))
            y = square.y + d * math.sin(math.radians(angle_deg))
            if height(x, y) > 120:
                return V(x, y, height(x, y))
        return V(square.x, square.y, height(square.x, square.y))

    placed = 0
    for i, (villager_id, villager) in enumerate(villagers.items()):
        if villager_id in here:
            continue
        bearing = i * 72.0 + 20.0
        stops = [dry(bearing, 1500.0), dry(bearing + 25.0, 500.0), dry(bearing - 40.0, 350.0), dry(bearing + 140.0, 650.0)]
        actor = subsystem.spawn_actor_from_class(unreal.Villager, stops[1], unreal.Rotator(0, 0, bearing))
        actor.set_editor_property("definition", villager)
        actor.set_editor_property("stops", stops)
        actor.set_actor_label(f"Villager_{villager_id}")
        actor.set_folder_path("Town")
        placed += 1
    if placed:
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(f"create_town: placed {placed} villagers.")


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
create_settlers()
place(create_traders())
place_board(create_board())
place_villagers(create_villagers())

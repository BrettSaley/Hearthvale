"""Sets up cooking and the character window's keys.

- Cooking data on the raw food items (what each cooks into, level, XP, burn level)
- The I (inventory) and K (skills) inputs, added to IMC_Hearthvale
- Placeholder materials in /Game/Cooking/Materials (a glowing flame, brick, iron)
- A campfire by the fishing pond and a range in the town corner of Lvl_ThirdPerson

Run headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_cooking.py"

The cooking numbers below are always written to the items (so edit them here, or
in the editor and don't re-run this). Other assets are left alone if they exist,
and stations are only placed if the level has none yet.
"""

import math
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


# --- Cooking data: raw item -> (cooked, burnt, level, xp, stop-burning level) ---

COOKING = {
    "RawPerch": ("Perch", "BurntFish", 1, 30, 25),
    "RawMeat": ("CookedMeat", "BurntMeat", 1, 30, 25),
    "RawTrout": ("Trout", "BurntFish", 15, 70, 45),
    "RawPike": ("Pike", "BurntFish", 30, 90, 60),
}


def set_cooking_data():
    for raw_name, (cooked, burnt, level, xp, stop) in COOKING.items():
        raw = unreal.load_asset(f"/Game/Items/{raw_name}")
        if raw is None:
            unreal.log_warning(f"create_cooking: no item {raw_name}; run create_items.py first.")
            continue
        raw.set_editor_property("cooked_item", unreal.load_asset(f"/Game/Items/{cooked}"))
        raw.set_editor_property("burnt_item", unreal.load_asset(f"/Game/Items/{burnt}"))
        raw.set_editor_property("cooking_level", level)
        raw.set_editor_property("cooking_xp", xp)
        raw.set_editor_property("stop_burning_level", stop)
        save(raw)


# --- Keys: I for the inventory, K for skills ---

def create_input():
    context = unreal.load_asset("/Game/Input/IMC_Hearthvale")
    for name, keys in [("IA_Inventory", ["I", "Gamepad_Special_Right"]), ("IA_Skills", ["K"])]:
        action, made = create(name, "/Game/Input/Actions", unreal.InputAction, unreal.InputAction_Factory())
        if not made:
            continue
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        save(action)
        for key_name in keys:
            key = unreal.Key()
            key.set_editor_property("key_name", key_name)
            context.map_key(action, key)
    save(context)


# --- Materials ---

MATERIALS = {}


def create_materials():
    flame, made = create("M_Flame", "/Game/Cooking/Materials", unreal.Material, unreal.MaterialFactoryNew())
    if made:
        flame.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        color = unreal.MaterialEditingLibrary.create_material_expression(flame, unreal.MaterialExpressionConstant3Vector, -300, 0)
        color.set_editor_property("constant", unreal.LinearColor(12.0, 3.2, 0.5, 1.0))
        unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(flame)
        save(flame)
    MATERIALS["Flame"] = flame

    parent = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    for name, (r, g, b) in {"Brick": (0.32, 0.1, 0.06), "RangeIron": (0.05, 0.05, 0.055)}.items():
        mi, made = create(f"MI_{name}", "/Game/Cooking/Materials", unreal.MaterialInstanceConstant,
                          unreal.MaterialInstanceConstantFactoryNew())
        if made:
            unreal.MaterialEditingLibrary.set_material_instance_parent(mi, parent)
            unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(r, g, b, 1.0))
            save(mi)
        MATERIALS[name] = mi
    for name in ["Stone", "Bark"]:
        MATERIALS[name] = unreal.load_asset(f"/Game/Gathering/Materials/MI_{name}")


# --- Station looks (engine basic shapes are 100 cm across, centred) ---

SHAPES = {s: unreal.load_asset(f"/Engine/BasicShapes/{s}") for s in ["Cylinder", "Sphere", "Cone", "Cube"]}


def part(shape, material, location, scale, rotation=(0, 0, 0)):
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", MATERIALS[material])
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    return p


def campfire_parts():
    stones = [part("Sphere", "Stone", (55 * math.cos(a), 55 * math.sin(a), 8), (0.32, 0.26, 0.2), (0, 0, math.degrees(a)))
              for a in [i * 2 * math.pi / 7 for i in range(7)]]
    logs = [part("Cylinder", "Bark", (0, 0, 10), (0.14, 0.14, 0.8), (90, 0, yaw)) for yaw in (30, 120)]
    fire = [part("Sphere", "Flame", (0, 0, 6), (0.55, 0.55, 0.12)),
            part("Cone", "Flame", (0, 0, 42), (0.42, 0.42, 0.65))]
    return stones + logs + fire


def range_parts():
    return [part("Cube", "Brick", (0, 0, 42), (1.1, 0.8, 0.84)),
            part("Cube", "RangeIron", (0, 0, 87), (1.16, 0.86, 0.06)),
            part("Cube", "Flame", (55.5, 0, 34), (0.03, 0.46, 0.26)),
            part("Cylinder", "RangeIron", (-38, 0, 160), (0.22, 0.22, 1.4)),
            part("Cylinder", "RangeIron", (15, 10, 101), (0.32, 0.32, 0.22))]


# --- Placing them ---

LEVEL = "/Game/ThirdPerson/Lvl_ThirdPerson"
# Campfire between the start and the fishing pond (x>0, y<0); range in the
# x>0, y>0 corner that's set aside for a town.
STATIONS = [
    dict(label="Campfire", name="Campfire", x=350, y=-300, burn=1.0, flicker=0.35, parts=campfire_parts),
    dict(label="Range", name="Range", x=700, y=700, burn=0.75, flicker=0.1, parts=range_parts),
]


def place_stations():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    if any(isinstance(a, unreal.CookingStation) for a in actors):
        unreal.log_warning("create_cooking: level already has cooking stations; not placing more.")
        return
    obstacles = []
    taken = []
    for a in actors:
        if isinstance(a, unreal.ResourceNode):
            loc = a.get_actor_location()
            taken.append((loc.x, loc.y))
            continue
        origin, extent = a.get_actor_bounds(False)
        if 20 < extent.x < 1500 and 20 < extent.y < 1500 and extent.z > 20:
            obstacles.append((origin, extent))

    def clear(x, y):
        off_blocks = all(abs(x - o.x) > e.x + 120 or abs(y - o.y) > e.y + 120 for o, e in obstacles)
        apart = all((x - tx) ** 2 + (y - ty) ** 2 > 400 ** 2 for tx, ty in taken)
        return abs(x) < 1700 and abs(y) < 1700 and off_blocks and apart

    offsets = [(0, 0)] + [(r * dx, r * dy) for r in range(100, 900, 100)
                          for dx, dy in [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)]]
    for s in STATIONS:
        spot = next(((s["x"] + dx, s["y"] + dy) for dx, dy in offsets if clear(s["x"] + dx, s["y"] + dy)), None)
        if spot is None:
            unreal.log_warning(f"create_cooking: no clear spot for the {s['name']}.")
            continue
        x, y = spot
        taken.append(spot)
        # Face the middle of the map (the range's oven door is on its +X side).
        yaw = math.degrees(math.atan2(-y, -x))
        station = actor_subsystem.spawn_actor_from_class(unreal.CookingStation, V(x, y, 0), unreal.Rotator(0, 0, yaw))
        station.set_editor_property("display_name", unreal.Text(s["name"]))
        station.set_editor_property("burn_chance_multiplier", s["burn"])
        station.set_editor_property("flicker", s["flicker"])
        station.set_editor_property("parts", s["parts"]())
        station.set_actor_label(s["label"])
        station.set_folder_path("Cooking")
        unreal.log_warning(f"create_cooking: placed the {s['name']} at ({x}, {y}).")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)


set_cooking_data()
create_input()
create_materials()
place_stations()

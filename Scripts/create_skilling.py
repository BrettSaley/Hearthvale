"""Sets up the gathering-skills update: hotbar keys, farming, bonus finds.

- Hotbar inputs: IA_Hotbar1-9 (number keys) and IA_HotbarScroll (mouse wheel),
  plus IA_Inventory (I) and IA_Skills (K), all in IMC_Hearthvale
- Crops in /Game/Farming/Crops, linked from their seed items
- Farm patches in the town corner of Lvl_ThirdPerson
- Bonus finds on gathering nodes (seeds from trees, gems from rocks) and their
  success messages; fishing spots give fish (there's no cooking)
- Removes what cooking left behind: raw and burnt items, the campfire and
  range, and their materials

Run create_items.py first (for the seed and crop items), then this, headless
with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_skilling.py"

The numbers below (crops, bonus finds) are always written, so edit them here
(or in the editor and don't re-run this). Patches are only placed if the level
has none.
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


def item(name):
    found = unreal.load_asset(f"/Game/Items/{name}")
    if found is None:
        raise RuntimeError(f"No item {name}; run create_items.py first.")
    return found


def key(name):
    k = unreal.Key()
    k.set_editor_property("key_name", name)
    return k


# --- Input ---

NUMBER_KEYS = ["One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine"]


def create_input():
    context = unreal.load_asset("/Game/Input/IMC_Hearthvale")
    actions = [("IA_Inventory", unreal.InputActionValueType.BOOLEAN, ["I", "Gamepad_Special_Right"]),
               ("IA_Skills", unreal.InputActionValueType.BOOLEAN, ["K"]),
               ("IA_HotbarScroll", unreal.InputActionValueType.AXIS1D, ["MouseWheelAxis"])]
    actions += [(f"IA_Hotbar{i + 1}", unreal.InputActionValueType.BOOLEAN, [k]) for i, k in enumerate(NUMBER_KEYS)]
    for name, value_type, keys in actions:
        action, made = create(name, "/Game/Input/Actions", unreal.InputAction, unreal.InputAction_Factory())
        if not made:
            continue
        action.set_editor_property("value_type", value_type)
        save(action)
        for k in keys:
            context.map_key(action, key(k))
    save(context)


# --- Materials ---

COLORS = {"Soil": (0.09, 0.055, 0.03), "Leaf": (0.12, 0.38, 0.06), "Potato": (0.45, 0.3, 0.14),
          "Onion": (0.85, 0.72, 0.45), "Cabbage": (0.32, 0.58, 0.18), "Pumpkin": (0.9, 0.38, 0.04)}
MATERIALS = {}


def create_materials():
    parent = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    for name, (r, g, b) in COLORS.items():
        mi, made = create(f"MI_{name}", "/Game/Farming/Materials", unreal.MaterialInstanceConstant,
                          unreal.MaterialInstanceConstantFactoryNew())
        if made:
            unreal.MaterialEditingLibrary.set_material_instance_parent(mi, parent)
            unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(r, g, b, 1.0))
            save(mi)
        MATERIALS[name] = mi


# The crop looks below need the materials, so make them (and the inputs) first.
create_input()
create_materials()

SHAPES = {s: unreal.load_asset(f"/Engine/BasicShapes/{s}") for s in ["Cylinder", "Sphere", "Cone", "Cube"]}


def part(shape, material, location, scale, rotation=(0, 0, 0)):
    """Basic shapes are 100 cm across, centred on their origin."""
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", MATERIALS[material])
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    return p


# --- Crops: (seed, produce, level, plant xp, harvest xp, grow seconds, harvests, next seed) ---

SPROUT = [part("Cone", "Leaf", (0, 0, 8), (0.12, 0.12, 0.16))]
LEAFY = [part("Sphere", "Leaf", (0, 0, 14), (0.38, 0.38, 0.26))]

CROPS = {
    "Potato": dict(name="Potatoes", seed="PotatoSeed", produce="Potato", level=1, plant_xp=8, harvest_xp=10,
                   grow=90, harvests=(3, 6), message="You dig up a potato.", next_seed=("OnionSeed", 30),
                   ready=[part("Sphere", "Leaf", (0, 0, 16), (0.4, 0.4, 0.28)), part("Sphere", "Potato", (18, 10, 6), (0.18, 0.15, 0.13))]),
    "Onion": dict(name="Onions", seed="OnionSeed", produce="Onion", level=10, plant_xp=14, harvest_xp=18,
                  grow=180, harvests=(3, 6), message="You pull up an onion.", next_seed=("CabbageSeed", 30),
                  ready=[part("Sphere", "Onion", (0, 0, 9), (0.22, 0.22, 0.2)), part("Cone", "Leaf", (0, 0, 28), (0.08, 0.08, 0.3))]),
    "Cabbage": dict(name="Cabbages", seed="CabbageSeed", produce="Cabbage", level=20, plant_xp=22, harvest_xp=28,
                    grow=300, harvests=(4, 7), message="You cut a cabbage.", next_seed=("PumpkinSeed", 30),
                    ready=[part("Sphere", "Cabbage", (0, 0, 16), (0.42, 0.42, 0.34)), part("Sphere", "Leaf", (0, 0, 8), (0.55, 0.55, 0.12))]),
    "Pumpkin": dict(name="Pumpkins", seed="PumpkinSeed", produce="Pumpkin", level=30, plant_xp=35, harvest_xp=45,
                    grow=480, harvests=(3, 5), message="You heave a pumpkin off the vine.", next_seed=None,
                    ready=[part("Sphere", "Pumpkin", (0, 0, 16), (0.4, 0.4, 0.32)), part("Sphere", "Leaf", (22, -18, 6), (0.35, 0.35, 0.1))]),
}


def drop(item_name, one_in, quantity=1):
    d = unreal.BonusDrop()
    d.set_editor_property("item", item(item_name))
    d.set_editor_property("one_in", one_in)
    d.set_editor_property("quantity", quantity)
    return d


def create_crops():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.CropDefinition)
    for asset_name, c in CROPS.items():
        crop, _ = create(asset_name, "/Game/Farming/Crops", unreal.CropDefinition, factory)
        for prop, value in [("display_name", unreal.Text(c["name"])), ("seed", item(c["seed"])), ("produce", item(c["produce"])),
                            ("required_level", c["level"]), ("plant_xp", c["plant_xp"]), ("harvest_xp", c["harvest_xp"]),
                            ("grow_seconds", float(c["grow"])), ("min_harvests", c["harvests"][0]),
                            ("max_harvests", c["harvests"][1]), ("harvest_message", unreal.Text(c["message"])),
                            ("bonus_drops", [drop(*c["next_seed"])] if c["next_seed"] else []),
                            ("seedling_parts", SPROUT), ("growing_parts", LEAFY), ("ready_parts", c["ready"])]:
            crop.set_editor_property(prop, value)
        save(crop)
        seed = item(c["seed"])
        seed.set_editor_property("crop", crop)
        save(seed)


# --- Gathering nodes: success messages and bonus finds ---

NODE_UPDATES = {
    # Trees drop seeds now and then (the better the tree, the better the seeds).
    "Tree": dict(drops=[("PotatoSeed", 12), ("OnionSeed", 40)]),
    "OakTree": dict(drops=[("OnionSeed", 15), ("CabbageSeed", 40)]),
    "AshTree": dict(drops=[("CabbageSeed", 15), ("PumpkinSeed", 40)]),
    # Rocks hide gems.
    "CopperRock": dict(message="You mine some {0}.", drops=[("UncutJade", 80)]),
    "IronRock": dict(message="You mine some {0}.", drops=[("UncutJade", 60), ("UncutAmber", 120)]),
    "SilverRock": dict(message="You mine some {0}.", drops=[("UncutAmber", 80), ("UncutSapphire", 200), ("UncutRuby", 600)]),
    # Fishing spots give raw fish (cooked on a fire; see create_crafting.py).
    "PerchSpot": dict(item="Perch", message="You catch a {0}.", drops=[]),
    "TroutSpot": dict(item="Trout", message="You catch a {0}.", drops=[]),
    "PikeSpot": dict(item="Pike", message="You catch a {0}.", drops=[]),
}


def update_nodes():
    for name, u in NODE_UPDATES.items():
        node = unreal.load_asset(f"/Game/Gathering/Nodes/{name}")
        if "item" in u:
            node.set_editor_property("item", item(u["item"]))
        if "message" in u:
            node.set_editor_property("success_message", unreal.Text(u["message"]))
        node.set_editor_property("bonus_drops", [drop(i, n) for i, n in u["drops"]])
        # Fishing spots lie on the water; walk through them.
        node.set_editor_property("blocks_movement", not name.endswith("Spot"))
        save(node)


# --- Clearing out cooking ---

OLD_ITEMS = ["RawPerch", "RawTrout", "RawPike", "RawMeat", "BurntFish", "BurntMeat"]


def remove_cooking_assets():
    for name in OLD_ITEMS:
        path = f"/Game/Items/{name}"
        if assets.does_asset_exist(path):
            assets.delete_asset(path)
    if assets.does_directory_exist("/Game/Cooking"):
        assets.delete_directory("/Game/Cooking")


# --- The level: farm patches in, cooking stations out ---

LEVEL = "/Game/ThirdPerson/Lvl_ThirdPerson"
# Two rows of two in the x>0, y>0 corner.
PATCHES = [(550, 520), (850, 520), (550, 780), (850, 780)]


def update_level():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()
    removed = 0
    for a in actors:
        if a.get_class().get_name() == "CookingStation":
            subsystem.destroy_actor(a)
            removed += 1
    if any(isinstance(a, unreal.FarmPatch) for a in actors):
        unreal.log_warning("create_skilling: level already has farm patches; not placing more.")
    else:
        for i, (x, y) in enumerate(PATCHES):
            patch = subsystem.spawn_actor_from_class(unreal.FarmPatch, V(x, y, 0), unreal.Rotator(0, 0, 0))
            patch.set_editor_property("soil_material", MATERIALS["Soil"])
            patch.set_actor_label(f"FarmPatch_{i}")
            patch.set_folder_path("Farming")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log_warning(f"create_skilling: removed {removed} cooking station(s); farm patches ready.")


create_crops()
update_nodes()
update_level()
remove_cooking_assets()

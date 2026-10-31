"""Sets up deep-water fishing: the fish you need a boat for, and what they make.

- Fish (raw, cooked at a fire): carp, salmon and char from the middle of the big
  lakes; mackerel and sea bass from the sea; tuna, swordfish and sharks far out
- Harpoons (iron, steel), smithed at an anvil: the big sea fish need one
- Better nets, knotted from wool at a workbench: a stout net and a trawl net catch more
- A pearl (a rare find out at sea) and a pearl ring to set it in
- Crabs and lobsters (caught by crab pots, built in create_building.py) and bait for them, cut from fish
- More fish ground into fertiliser at the workbench
- Fishing spot definitions in /Game/Gathering/Nodes for the deep-water grounds.
  They aren't placed in the level: the game places them when it starts, where
  UWorldGenerator::PlanFishingGrounds says (lakes by biome, the sea by depth)

The boats themselves are build pieces, in create_building.py.

Run after create_items.py, create_gathering.py, create_skilling.py and
create_crafting.py, and before create_building.py, headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_fishing.py"

Everything is rewritten from the tables below, so edit it here.
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
V = unreal.Vector
Cat = unreal.ItemCategory
Station = unreal.CraftStation


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
        raise RuntimeError(f"No item {name}.")
    return found


# --- Items ---
# Fish: (name, examine, value, cooked name, cooked examine, cooked value, heals)
FISH = {
    "Carp": ("Raw carp", "Fat and golden, from the lake's deep middle. Cook it on a fire.", 30,
             "Baked carp", "Earthy, but filling.", 55, 8),
    "Mackerel": ("Raw mackerel", "Tiger-striped and oily. Cook it on a fire.", 40,
                 "Grilled mackerel", "Smoky and rich.", 70, 9),
    "Salmon": ("Raw salmon", "A great silver salmon. Cook it on a fire.", 70,
               "Grilled salmon", "Pink, flaky and perfect.", 120, 12),
    "SeaBass": ("Raw sea bass", "A sleek grey bass. Cook it on a fire.", 85,
                "Grilled sea bass", "Delicate and white.", 140, 13),
    "Char": ("Raw char", "An arctic char from a cold mountain tarn. Cook it on a fire.", 110,
             "Grilled char", "Like salmon, only better.", 180, 14),
    "Tuna": ("Raw tuna", "A huge tuna. Heavy as a sack of ore. Cook it on a fire.", 150,
             "Tuna steak", "Thick, seared and red in the middle.", 240, 16),
    "Swordfish": ("Raw swordfish", "Its bill is longer than your arm. Cook it on a fire.", 240,
                  "Swordfish steak", "Meaty and mild.", 380, 19),
    "Crab": ("Raw crab", "It's stopped pinching. Cook it on a fire.", 25,
             "Boiled crab", "Sweet white meat, if you can get at it.", 45, 7),
    "Lobster": ("Raw lobster", "Big, blue and cross. Cook it on a fire.", 120,
                "Boiled lobster", "A feast in a shell.", 200, 15),
    "Shark": ("Raw shark", "All teeth and gristle, and quite dead. Probably. Cook it on a fire.", 380,
              "Shark steak", "A trophy of a meal.", 600, 22),
}

ITEMS = {
    # Harpoons: the tool for the big sea fish (power improves the catch rate, as a better axe does).
    "IronHarpoon": dict(name="Iron harpoon", examine="Barbed, weighted and on a long line.", category=Cat.TOOL,
                        value=180, harpoon=1, requires=50),
    "SteelHarpoon": dict(name="Steel harpoon", examine="Nothing in the sea gets away from this.", category=Cat.TOOL,
                         value=600, harpoon=3, requires=70),
    # Nets: a better one catches more (power, as with any tool); knotted from wool.
    "StoutNet": dict(name="Stout net", examine="Wool cord, tightly knotted. Fewer fish get away.", category=Cat.TOOL,
                     value=60, net=2, requires=25),
    "TrawlNet": dict(name="Trawl net", examine="A great weighted net. Hardly anything gets away.", category=Cat.TOOL,
                     value=220, net=4, requires=55),
    "Bait": dict(name="Bait", examine="Chopped-up fish. Crabs love it; nobody else does.", category=Cat.RESOURCE,
                 stackable=True, value=2),
    "Pearl": dict(name="Pearl", examine="A perfect pearl, found out at sea.", category=Cat.VALUABLE, stackable=True, value=400),
    "PearlRing": dict(name="Pearl ring", examine="Silver set with a sea pearl.", category=Cat.VALUABLE, value=900,
                      slot=unreal.EquipSlot.RING),
}


def create_items():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ItemDefinition)
    entries = dict(ITEMS)
    for fish, (name, examine, value, cooked, cooked_examine, cooked_value, heal) in FISH.items():
        entries[fish] = dict(name=name, examine=examine, category=Cat.RESOURCE, value=value)
        entries[cooked_id(fish)] = dict(name=cooked, examine=cooked_examine, category=Cat.FOOD, value=cooked_value, heal=heal)
    for item_id, data in entries.items():
        made, _ = create(item_id, "/Game/Items", unreal.ItemDefinition, factory)
        for prop, value in [("display_name", unreal.Text(data["name"])), ("examine", unreal.Text(data["examine"])),
                            ("category", data["category"]), ("stackable", data.get("stackable", False)),
                            ("value", data["value"]), ("heal_amount", data.get("heal", 0)),
                            ("equip_slot", data.get("slot", unreal.EquipSlot.NONE))]:
            made.set_editor_property(prop, value)
        if "net" in data:
            made.set_editor_property("tool_type", unreal.ToolType.FISHING_NET)
            made.set_editor_property("tool_power", data["net"])
            made.set_editor_property("requirements", {unreal.Skill.FISHING: data["requires"]})
        if "harpoon" in data:
            made.set_editor_property("tool_type", unreal.ToolType.HARPOON)
            made.set_editor_property("tool_power", data["harpoon"])
            made.set_editor_property("requirements", {unreal.Skill.FISHING: data["requires"]})
        save(made)


def cooked_id(fish):
    return {"Tuna": "TunaSteak", "Swordfish": "SwordfishSteak", "Shark": "SharkSteak", "Carp": "BakedCarp",
            "Crab": "BoiledCrab", "Lobster": "BoiledLobster"}.get(fish, f"Grilled{fish}")


# --- Recipes: name: (station, [(item, count)], output, count, seconds, message[, (Smithing level, XP)]) ---

RECIPES = {f"Cook{fish}": (Station.FIRE, [(fish, 1)], cooked_id(fish), 1, 2.4 if fish in ("Tuna", "Swordfish", "Shark") else 1.8,
                           f"You cook the {FISH[fish][0].split(' ', 1)[1]}.") for fish in FISH}
RECIPES.update({
    # Fishing feeds farming: the bigger the fish, the more fertiliser.
    "GrindCarp": (Station.WORKBENCH, [("Carp", 1)], "Fertiliser", 6, 1.8, "You grind a carp into fertiliser."),
    "GrindMackerel": (Station.WORKBENCH, [("Mackerel", 1)], "Fertiliser", 6, 1.8, "You grind a mackerel into fertiliser."),
    "GrindShark": (Station.WORKBENCH, [("Shark", 1)], "Fertiliser", 25, 3.0, "You grind a whole shark into fertiliser. Grim, but the crops will love it."),
    # Nets from the farm's wool.
    "KnotStoutNet": (Station.WORKBENCH, [("Wool", 4)], "StoutNet", 1, 3.0, "You knot a stout net."),
    "KnotTrawlNet": (Station.WORKBENCH, [("Wool", 10), ("IronBar", 1)], "TrawlNet", 1, 4.0, "You knot a trawl net and weight it with iron."),
    # Bait for crab pots, cut from the little fish.
    "CutPerchBait": (Station.WORKBENCH, [("Perch", 1)], "Bait", 6, 1.2, "You cut a perch up for bait."),
    "CutMackerelBait": (Station.WORKBENCH, [("Mackerel", 1)], "Bait", 12, 1.2, "You cut a mackerel up for bait."),
    # Harpoons and the pearl ring train Smithing.
    "SmithIronHarpoon": (Station.ANVIL, [("IronBar", 3), ("OakPlank", 1)], "IronHarpoon", 1, 3.0,
                         "You hammer out an iron harpoon.", (18, 140)),
    "SmithSteelHarpoon": (Station.ANVIL, [("SteelBar", 3), ("AshPlank", 1)], "SteelHarpoon", 1, 3.0,
                          "You hammer out a steel harpoon.", (38, 260)),
    "SmithPearlRing": (Station.ANVIL, [("SilverBar", 1), ("Pearl", 1)], "PearlRing", 1, 3.0,
                       "You set the pearl in a silver ring.", (45, 320)),
})


def create_recipes():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.RecipeDefinition)
    for order, (asset_name, (station, inputs, output, count, seconds, message, *smithing)) in enumerate(RECIPES.items()):
        recipe, _ = create(asset_name, "/Game/Crafting/Recipes", unreal.RecipeDefinition, factory)
        amounts = []
        for name, quantity in inputs:
            a = unreal.ItemAmount()
            a.set_editor_property("item", item(name))
            a.set_editor_property("quantity", quantity)
            amounts.append(a)
        for prop, value in [("station", station), ("inputs", amounts), ("output", item(output)),
                            ("output_quantity", count), ("seconds", seconds), ("made_message", unreal.Text(message)),
                            ("sort_order", 80 + order)]:
            recipe.set_editor_property(prop, value)
        if smithing:
            recipe.set_editor_property("skill", unreal.Skill.SMITHING)
            recipe.set_editor_property("required_level", smithing[0][0])
            recipe.set_editor_property("xp", smithing[0][1])
        save(recipe)


# --- The deep-water fishing spots ---

SHAPES = {name: unreal.load_asset(f"/Engine/BasicShapes/{name}") for name in ["Cylinder", "Sphere"]}


def part(shape, material, location, scale):
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", unreal.load_asset(f"/Game/Gathering/Materials/MI_{material}"))
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(0, 0, 0), V(*scale)))
    p.set_editor_property("collides", True)
    return p


def pool(size):
    # Churning water: a dark disc with a ring of foam, bigger the further out.
    return [part("Cylinder", "Water", (0, 0, 2), (2.2 * size, 2.2 * size, 0.04)),
            part("Cylinder", "Foam", (0, 0, 3), (0.9 * size, 0.9 * size, 0.03)),
            part("Cylinder", "Foam", (60 * size, 30 * size, 3), (0.35 * size, 0.35 * size, 0.03))]


Skill, Tool = unreal.Skill, unreal.ToolType
# kind: (name, verb, start message, level, tool, fish, xp, difficulty, look size, [(rare find, 1 in)])
NODES = {
    "CarpSpot": ("Deep water", "Net", "You cast your net out over the deep water.", 15, Tool.FISHING_NET, "Carp", 35, 0.05, 1.2, []),
    "MackerelSpot": ("Shoal", "Net", "You cast your net over the shoal.", 25, Tool.FISHING_NET, "Mackerel", 45, 0.05, 1.4,
                     [("Pearl", 400)]),
    "SalmonSpot": ("Deep water", "Net", "You cast your net out over the deep water.", 35, Tool.FISHING_NET, "Salmon", 65, 0.1, 1.2, []),
    "BassSpot": ("Shoal", "Net", "You cast your net over the shoal.", 40, Tool.FISHING_NET, "SeaBass", 75, 0.1, 1.4,
                 [("Pearl", 250)]),
    "CharSpot": ("Deep water", "Net", "You cast your net out over the cold water.", 50, Tool.FISHING_NET, "Char", 90, 0.12, 1.2, []),
    "TunaSpot": ("Open sea", "Harpoon", "You ready your harpoon and watch the water.", 55, Tool.HARPOON, "Tuna", 100, 0.12, 1.8,
                 [("Pearl", 150)]),
    "SwordfishSpot": ("Open sea", "Harpoon", "You ready your harpoon and watch the water.", 70, Tool.HARPOON, "Swordfish", 130, 0.15, 1.8,
                      [("Pearl", 120)]),
    "SharkSpot": ("Open sea", "Harpoon", "A fin circles. You ready your harpoon.", 85, Tool.HARPOON, "Shark", 170, 0.2, 2.0,
                  [("Pearl", 80)]),
}


def create_nodes():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GatheringNodeDefinition)
    for kind, (name, verb, start, level, tool, fish, xp, difficulty, size, rare) in NODES.items():
        node, _ = create(kind, "/Game/Gathering/Nodes", unreal.GatheringNodeDefinition, factory)
        drops = []
        for drop_item, one_in in rare:
            d = unreal.BonusDrop()
            d.set_editor_property("item", item(drop_item))
            d.set_editor_property("one_in", one_in)
            d.set_editor_property("quantity", 1)
            drops.append(d)
        for prop, value in [("display_name", unreal.Text(name)), ("action_verb", unreal.Text(verb)),
                            ("start_message", unreal.Text(start)), ("success_message", unreal.Text("You catch a {0}.")),
                            ("skill", Skill.FISHING), ("required_level", level), ("tool", tool), ("item", item(fish)),
                            ("xp", xp), ("attempt_interval", 3.0), ("difficulty", difficulty), ("deplete_chance", 0.0),
                            ("respawn_seconds", 0.0), ("bonus_drops", drops), ("blocks_movement", False),
                            ("parts", pool(size)), ("depleted_parts", [])]:
            node.set_editor_property(prop, value)
        save(node)
    unreal.log(f"create_fishing: wrote {len(NODES)} fishing spots.")


create_items()
create_recipes()
create_nodes()

"""Sets up traps for wild animals, built from the Traps tab of build mode.

- Snare: twine on a stake, set where rabbits and foxes roam. Now and then it
  catches one in range, and what it would drop (meat, a pelt) waits in it.
- Wooden and iron spikes: sharpened stakes that hurt boars, wolves and bears
  that blunder into them, until they're smashed.

None needs a workbench nearby, so they can be set out in the wild. How they
work is in UTrapComponent.

Run after create_building.py (for its materials), headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_traps.py"

Pieces are always written, so edit them here.
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
V = unreal.Vector
FOLDER = "/Game/Building/Pieces"
SHAPES = {s: unreal.load_asset(f"/Engine/BasicShapes/{s}") for s in ["Cylinder", "Sphere", "Cone", "Cube"]}
MATERIALS = {m: unreal.load_asset(f"/Game/Building/MI_Build{m}") for m in ["Planks", "Straw", "Iron", "Leather"]}


def part(shape, material, location, scale, rotation=(0, 0, 0)):
    """Basic shapes are 100 cm across, centred on their origin. Rotation is (roll, pitch, yaw). Traps don't block anyone."""
    p = unreal.NodeMeshPart()
    p.set_editor_property("mesh", SHAPES[shape])
    p.set_editor_property("material", MATERIALS[material])
    p.set_editor_property("transform", unreal.Transform(V(*location), unreal.Rotator(*rotation), V(*scale)))
    p.set_editor_property("collides", False)
    p.set_editor_property("visible", True)
    return p


def snare():
    # A stake with a bent sapling and a twine loop lying on the ground.
    return [part("Cylinder", "Planks", (0, 0, 20), (0.05, 0.05, 0.4)),
            part("Cylinder", "Planks", (18, 0, 40), (0.03, 0.03, 0.45), (0, 50, 0)),
            part("Cylinder", "Straw", (40, 0, 22), (0.01, 0.01, 0.4), (0, 15, 0)),
            part("Cylinder", "Straw", (40, 0, 2), (0.3, 0.3, 0.015))]


def spikes(tip):
    # A low log with sharpened stakes leaning outwards.
    parts = [part("Cylinder", "Planks", (0, 0, 10), (0.2, 0.2, 1.6), (90, 0, 0))]
    for i, y in enumerate((-60, -30, 0, 30, 60)):
        lean = 25 if i % 2 else -25
        parts.append(part("Cylinder", "Planks", (lean * 0.6, y, 40), (0.07, 0.07, 0.6), (0, lean, 0)))
        parts.append(part("Cone", tip, (lean * 1.25, y, 72), (0.09, 0.09, 0.16), (0, lean, 0)))
    return parts


# ID: (name, cost, parts, footprint, height, sort, settings)
TRAPS = {
    "Snare": ("Snare", [("Wool", 1), ("Plank", 1)], snare(), (30, 30, 15), 20, 300,
              dict(trap=unreal.TrapKind.SNARE, storage=4, range=700.0, work=15.0, chance=0.3,
                   about="Set it where rabbits and foxes roam; now and then it catches one. Collect the catch from it.")),
    "WoodenSpikes": ("Wooden spikes", [("Plank", 4)], spikes("Planks"), (40, 80, 40), 80, 301,
                     dict(trap=unreal.TrapKind.SPIKES, range=170.0, damage=3, uses=15,
                          about="Hurts boars, wolves and bears that blunder in. Breaks after 15 blows.")),
    "IronSpikes": ("Iron spikes", [("Plank", 2), ("IronBar", 2)], spikes("Iron"), (40, 80, 40), 80, 302,
                   dict(trap=unreal.TrapKind.SPIKES, range=170.0, damage=6, uses=40, smithing=12,
                        about="Iron-tipped and far tougher. Breaks after 40 blows.")),
}


def item(name):
    found = unreal.load_asset(f"/Game/Items/{name}")
    if found is None:
        raise RuntimeError(f"No item {name}.")
    return found


def main():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.BuildPieceDefinition)
    for piece_id, (name, cost, parts, footprint, height, sort, s) in TRAPS.items():
        path = f"{FOLDER}/{piece_id}"
        piece = unreal.load_asset(path) if assets.does_asset_exist(path) else \
            asset_tools.create_asset(piece_id, FOLDER, unreal.BuildPieceDefinition, factory)
        amounts = []
        for item_id, count in cost:
            a = unreal.ItemAmount()
            a.set_editor_property("item", item(item_id))
            a.set_editor_property("quantity", count)
            amounts.append(a)
        for prop, value in [("display_name", unreal.Text(name)), ("category", unreal.BuildCategory.TRAPS),
                            ("snap", unreal.BuildSnap.FREE), ("cost", amounts), ("parts", parts),
                            ("footprint", V(*footprint)), ("height", float(height)), ("sort_order", sort),
                            ("needs_workbench", False), ("description", unreal.Text(s["about"])),
                            ("storage_slots", s.get("storage", 0)), ("trap", s["trap"]),
                            ("range", s["range"]), ("work_seconds", s.get("work", 2.0)),
                            ("catch_chance", s.get("chance", 0.0)), ("trap_damage", s.get("damage", 0)),
                            ("trap_uses", s.get("uses", 0)), ("required_level", s.get("smithing", 1))]:
            piece.set_editor_property(prop, value)
        if "smithing" in s:  # (No skill is the default, and Python can't name it.)
            piece.set_editor_property("required_skill", unreal.Skill.SMITHING)
        assets.save_loaded_asset(piece, only_if_is_dirty=False)
    unreal.log_warning(f"create_traps: wrote {len(TRAPS)} trap pieces.")


main()

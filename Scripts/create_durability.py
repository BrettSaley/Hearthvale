"""Sets how long tools, weapons and armour last, and what mends them.

Each item gets a number of uses (a gather, a swing or shot, a blow taken)
before it breaks (FDurability), a repair material and the station it's mended
at: metal at an anvil with a bar of the same metal, wood and leather at a
workbench with a plank or a hide. Better materials last longer.

Run last of the item scripts (after create_items.py, create_crafting.py,
create_farming.py, create_fishing.py, create_hunting.py, create_armour.py and
create_weapons.py),
headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_durability.py"

Always written, so edit the numbers here.
"""

import unreal

assets = unreal.EditorAssetLibrary
A, W = unreal.CraftStation.ANVIL, unreal.CraftStation.WORKBENCH

# Uses by material, and what mends it where.
METALS = {"Copper": (120, "CopperBar"), "Iron": (250, "IronBar"), "Steel": (500, "SteelBar")}
METAL_KINDS = ["Axe", "Pickaxe", "Sickle", "Harpoon", "Sword", "Helm", "Chestplate", "Greaves", "Shield"]

# Item: (uses, repair material, station)
OTHERS = {
    "FishingNet": (150, "Wool", W), "StoutNet": (250, "Wool", W), "TrawlNet": (400, "Wool", W),
    "Shortbow": (150, "Plank", W), "OakLongbow": (300, "OakPlank", W), "AshLongbow": (500, "AshPlank", W),
    "WoodenShield": (60, "Plank", W),
    "LeatherCap": (90, "Hide", W), "LeatherJerkin": (90, "Hide", W), "LeatherTrousers": (90, "Hide", W),
    "LeatherGloves": (90, "Hide", W), "LeatherBoots": (90, "Hide", W),
    "SilversteelHelm": (750, "SilverBar", A), "SilversteelChestplate": (750, "SilverBar", A),
    "SilversteelGreaves": (750, "SilverBar", A), "SilversteelShield": (750, "SilverBar", A),
}


def load(name):
    path = f"/Game/Items/{name}"
    return unreal.load_asset(path) if assets.does_asset_exist(path) else None


def main():
    table = dict(OTHERS)
    for metal, (uses, bar) in METALS.items():
        for kind in METAL_KINDS:
            if metal + kind != "CopperHarpoon":  # (Harpoons start at iron.)
                table[metal + kind] = (uses, bar, A)
    done, missing = 0, []
    for item_id, (uses, material, station) in table.items():
        item, mend = load(item_id), load(material)
        if item is None or mend is None:
            missing.append(item_id)
            continue
        item.set_editor_property("durability", uses)
        item.set_editor_property("repair_item", mend)
        item.set_editor_property("repair_station", station)
        assets.save_loaded_asset(item, only_if_is_dirty=False)
        done += 1
    unreal.log_warning(f"create_durability: set {done} item(s); not found: {', '.join(missing) or 'none'}.")


main()

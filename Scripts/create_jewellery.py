"""Gives the rings smithed at the anvil something to do when worn.

Rings go in the ring slot, but until now they did nothing there. Each now adds
a little to aim (accuracy), weight of blow (strength) or defence; the rarer
the gem, the more. Amulets from alpha trophies are in create_trophies.py.

Run after create_items.py, headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_jewellery.py"

Always written, so edit the numbers here.
"""

import unreal

assets = unreal.EditorAssetLibrary

# Ring: (accuracy, strength, defence)
RINGS = {
    "SilverRing": (0, 0, 1),
    "JadeRing": (2, 0, 1),
    "AmberRing": (0, 2, 1),
    "SapphireRing": (1, 1, 3),
    "RubyRing": (3, 3, 2),
}


def main():
    done = 0
    for ring_id, (accuracy, strength, defence) in RINGS.items():
        path = f"/Game/Items/{ring_id}"
        if not assets.does_asset_exist(path):
            unreal.log_warning(f"create_jewellery: no {ring_id}.")
            continue
        ring = unreal.load_asset(path)
        ring.set_editor_property("equip_slot", unreal.EquipSlot.RING)
        ring.set_editor_property("accuracy_bonus", accuracy)
        ring.set_editor_property("strength_bonus", strength)
        ring.set_editor_property("defence_bonus", defence)
        assets.save_loaded_asset(ring, only_if_is_dirty=False)
        done += 1
    unreal.log_warning(f"create_jewellery: set {done} ring(s).")


main()

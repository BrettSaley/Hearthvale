"""Makes the settler's action montages (attacks, flinches, blocks, death, tool swings).

Each is a montage on the default slot (which ABP_Unarmed plays over walking),
made from the mannequin animations that come with the project, in
/Game/Characters/Settler/Anims; the code plays them by name (HearthvaleAnims::Play):

- Swing1/Swing2: sword and fist blows, alternating
- Thrust: a spear's lunge
- Loose: a bow shot; Crossbow: a crossbow shot
- Flinch1-3: a light hit; Stagger: a heavy one; Block: catching a blow on a shield
- Death1-3: falling (held at the end until they wake)
- Chop: felling, mining and mending swings (also set as the gather montage of
  every tree, rock and crop node, so gathering plays it)

Run after create_gathering.py and create_skilling.py, headless with the editor closed:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/create_animations.py"

Montages are always rebuilt, so edit them here.
"""

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
FOLDER = "/Game/Characters/Settler/Anims"
ANIMS = "/Game/Characters/Mannequins/Anims"

# Montage: (source animation, rate, hold the last pose)
MONTAGES = {
    "Swing1": ("Unarmed/Attack/MM_Attack_01", 1.3, False),
    "Swing2": ("Unarmed/Attack/MM_Attack_02", 1.3, False),
    "Thrust": ("Unarmed/Attack/MM_Attack_03", 1.1, False),
    "Loose": ("Pistol/MM_Pistol_Fire", 0.8, False),
    "Crossbow": ("Rifle/MM_Rifle_Fire", 0.8, False),
    "Flinch1": ("Rifle/HitReact/MM_HitReact_Front_Lgt_01", 1.2, False),
    "Flinch2": ("Rifle/HitReact/MM_HitReact_Front_Lgt_02", 1.2, False),
    "Flinch3": ("Rifle/HitReact/MM_HitReact_Front_Lgt_03", 1.2, False),
    "Stagger": ("Rifle/HitReact/MM_HitReact_Front_Hvy_01", 1.0, False),
    "Block": ("Rifle/HitReact/MM_HitReact_Front_Med_01", 1.4, False),
    "Death1": ("Death/MM_Death_Front_01", 1.0, True),
    "Death2": ("Death/MM_Death_Front_02", 1.0, True),
    "Death3": ("Death/MM_Death_Back_01", 1.0, True),
    "Chop": ("Unarmed/Attack/MM_Attack_01", 0.9, False),
}


def make_montage(name, source, rate, hold):
    path = f"{FOLDER}/AM_{name}"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    sequence = unreal.load_asset(f"{ANIMS}/{source}")
    if sequence is None:
        raise RuntimeError(f"No animation {source}.")
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", sequence)
    montage = asset_tools.create_asset(f"AM_{name}", FOLDER, unreal.AnimMontage, factory)
    montage.set_editor_property("rate_scale", rate)
    if hold:
        montage.set_editor_property("enable_auto_blend_out", False)
    assets.save_loaded_asset(montage, only_if_is_dirty=False)
    return montage


def set_gather_montages(chop):
    # Trees, rocks and crops play the chop swing while being worked (fishing spots stay still).
    tools = (unreal.ToolType.AXE, unreal.ToolType.PICKAXE, unreal.ToolType.SICKLE)
    count = 0
    for path in assets.list_assets("/Game/Gathering/Nodes", recursive=True, include_folder=False):
        node = unreal.load_asset(path)
        if isinstance(node, unreal.GatheringNodeDefinition) and node.get_editor_property("tool") in tools:
            node.set_editor_property("gather_montage", chop)
            assets.save_loaded_asset(node, only_if_is_dirty=False)
            count += 1
    return count


def main():
    made = {name: make_montage(name, *settings) for name, settings in MONTAGES.items()}
    nodes = set_gather_montages(made["Chop"])
    unreal.log_warning(f"create_animations: made {len(made)} montages; {nodes} gathering nodes swing.")


main()

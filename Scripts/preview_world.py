"""Draws the island from above without opening the level: Saved/WorldPreview/map.png.

Shaded ground and water, every resource node as a dot (greens: trees,
orange/red/white: copper/iron/silver, cyan/blue: fishing), grey: boulders and
fallen logs, red: the start. Also logs how many of each node and piece of
scenery there are. For checking a change to UWorldGenerator before rebuilding
the level.

Run headless:
    UnrealEditor-Cmd.exe Hearthvale.uproject -run=pythonscript -script="<path>/Scripts/preview_world.py"
"""

import os

import unreal

folder = os.path.join(unreal.Paths.project_saved_dir(), "WorldPreview")
os.makedirs(folder, exist_ok=True)

nodes = {}
for spot in unreal.WorldGenerator.plan_resources():
    nodes[str(spot.kind)] = nodes.get(str(spot.kind), 0) + 1
scenery = {}
for spot in unreal.WorldGenerator.plan_scenery():
    scenery[str(spot.kind)] = scenery.get(str(spot.kind), 0) + 1

unreal.WorldGenerator.save_map_preview(os.path.join(folder, "map.png"), 1024)
# Close-ups (centre x, y and width in metres): the start, a stretch of Deepwood, the Highlands.
for name, x, y, width in [("start", 0, 0, 260), ("deepwood", -430, 80, 320), ("highlands", 60, 660, 320)]:
    unreal.WorldGenerator.save_map_preview(os.path.join(folder, f"zoom_{name}.png"), 768, x, y, width)
unreal.log_warning(f"preview_world: nodes {sum(nodes.values())} {dict(sorted(nodes.items()))}")
unreal.log_warning(f"preview_world: scenery {sum(scenery.values())} {dict(sorted(scenery.items()))}")

"""Takes a set of screenshots around the island, for looking over the world.

Opens /Game/World/Lvl_World, switches the viewport to game view, flies the
camera to each view below and saves a screenshot of it to
Saved/Screenshots/WindowsEditor. Birds are posed but don't fly in the editor.

Needs the full editor (it renders):
    UnrealEditor.exe Hearthvale.uproject -ExecutePythonScript="<path>/Scripts/screenshot_world.py"
It quits the editor when done if launched with -QuitAfterShots.
"""

import math
import time

import unreal

V = unreal.Vector
gen = unreal.WorldGenerator


def ground(x, y):
    return max(gen.get_ground_height(x * 100, y * 100), 0) / 100


def look(name, x, y, height, at_x, at_y, at_height):
    """A view from (x, y) at height m above the ground/water, looking at a point at at_height m above its ground (all metres)."""
    eye = V(x * 100, y * 100, (ground(x, y) + height) * 100)
    target = V(at_x * 100, at_y * 100, (ground(at_x, at_y) + at_height) * 100)
    d = target - eye
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
    return name, eye, unreal.Rotator(0, pitch, yaw)


VIEWS = [
    look("01_start", -45, -45, 4, 15, 10, 1),
    look("02_starter_grove", -10, -5, 2, -40, -30, 3),
    look("03_meadow_lake", 95, 75, 3, 150, 120, 0),
    look("04_deepwood", -400, -60, 2, -440, -150, 4),
    look("05_big_lake", 380, 120, 25, 480, 190, 0),
    look("06_highlands", 120, 470, 30, 250, 650, 30),
    look("07_coast", 0, -700, 40, 160, -830, 20),
    look("08_overview", 0, -950, 350, 0, 0, 0),
    look("09_west_river", -760, 150, 12, -600, 185, 0),
    look("10_beach", 470, -650, 6, 560, -790, 0),
    look("11_east_river", 300, 120, 8, 480, 190, 0),
    look("12_rabbits", -4, -6, 1.2, 8, -18, 0.3),
]
# The first bridge, seen from a little way up the bank.
for spot in gen.plan_scenery():
    if str(spot.kind) == "Bridge":
        b = spot.transform.translation
        VIEWS.append(look("13_bridge", b.x / 100 + 18, b.y / 100 + 14, 5, b.x / 100, b.y / 100, 0))
        break
# The landmarks, from a little way off.
for kind, name, dx, dy, up in [("StandingStone", "14_stone_circle", 22, 16, 4), ("TowerWall", "15_watchtower", 26, -20, 8)]:
    for spot in gen.plan_scenery():
        if str(spot.kind) == kind:
            t = spot.transform.translation
            VIEWS.append(look(name, t.x / 100 + dx, t.y / 100 + dy, up, t.x / 100, t.y / 100, 2))
            break

state = {"start": time.monotonic(), "step": -1, "next": 25.0, "handle": None}


def tick(dt):
    now = time.monotonic() - state["start"]
    if now < state["next"]:
        return
    state["step"] += 1
    step = state["step"]
    # Each view: move the camera, give it a few seconds to load and settle, then shoot.
    if step < len(VIEWS) * 2:
        name, eye, rotation = VIEWS[step // 2]
        if step % 2 == 0:
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(eye, rotation)
            state["next"] = now + 4.0
        else:
            unreal.SystemLibrary.execute_console_command(None, f"HighResShot 1600x900 filename={name}")
            unreal.log_warning(f"screenshot_world: {name}")
            state["next"] = now + 3.0
        return
    unreal.unregister_slate_post_tick_callback(state["handle"])
    unreal.log_warning("screenshot_world: done.")
    if "-quitaftershots" in unreal.SystemLibrary.get_command_line().lower():
        unreal.SystemLibrary.quit_editor()


unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("/Game/World/Lvl_World")
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True)
unreal.SystemLibrary.execute_console_command(None, "r.ScreenPercentage 100")
state["handle"] = unreal.register_slate_post_tick_callback(tick)

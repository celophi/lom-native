from __future__ import annotations

import argparse
import functools
import json
import struct
from pathlib import Path

import lom_saves
import lom_scenes

from .settings import BASELINE_SUFFIX, COMPANION_VARIANT, EVENT_WATCH_FRAMES, GUEST_VARIANT, LAND_DISTANCE, LAND_PLACED_FLAGS, NO_GUEST, STORY_ROUTE_TRIES, TALK_ENTRY_TRIES, WALKED_INTO_SCENE_CHANGE, WMAP_LANDS, WMAP_LAYOUT_BYTES, WMAP_LAYOUT_FILES, WMAP_LAYOUT_RESOURCE, WMAP_NO_SCENE, WMAP_SCENES_OFFSET, WMAP_SCENE_VARIANTS
from .plans import guest_join_scenes
from .comparisons import record_route
from .generate_scenes import area_scenes, script_regions, story_triggers


LAND_CELL_NONE = 15
WMAP_GRID_SIZE = 6
REGION_FAR_EDGE = 480
REGION_WHOLE_MAP = 999
REGION_MIDDLE = 160
REGION_PUSH_FRAMES = 90


@functools.lru_cache(maxsize=None)
def area_land(disc: Path, area: str) -> int | None:
    """The world map land whose scenes are an area's (the first in the layouts' scene tables), or None."""
    paths = lom_scenes.resource_paths()
    for resource in range(WMAP_LAYOUT_RESOURCE, WMAP_LAYOUT_RESOURCE + WMAP_LAYOUT_FILES):
        if resource not in paths or not (disc / paths[resource]).exists():
            continue
        data = (disc / paths[resource]).read_bytes()
        if len(data) != WMAP_LAYOUT_BYTES:
            continue
        for index, scene in enumerate(struct.unpack_from(f"<{WMAP_LANDS * WMAP_SCENE_VARIANTS}H", data, WMAP_SCENES_OFFSET)):
            number = scene & lom_scenes.SCENE_MASK
            if (scene not in WMAP_NO_SCENE and number < lom_scenes.SCENE_COUNT
                    and Path(paths[lom_scenes.SCENE_RESOURCE_BASE + number]).stem.split("_")[0] == area):
                return index // WMAP_SCENE_VARIANTS
    return None


def area_lands(disc: Path) -> dict[str, set[int]]:
    """Every land whose world map entries lead into each area (WST is both land 14's and land 18's, say)."""
    paths = lom_scenes.resource_paths()
    out: dict[str, set[int]] = {}
    for resource in range(WMAP_LAYOUT_RESOURCE, WMAP_LAYOUT_RESOURCE + WMAP_LAYOUT_FILES):
        if resource not in paths or not (disc / paths[resource]).exists() or len(data := (disc / paths[resource]).read_bytes()) != WMAP_LAYOUT_BYTES:
            continue
        for index, scene in enumerate(struct.unpack_from(f"<{WMAP_LANDS * WMAP_SCENE_VARIANTS}H", data, WMAP_SCENES_OFFSET)):
            number = scene & lom_scenes.SCENE_MASK
            if scene not in WMAP_NO_SCENE and number < lom_scenes.SCENE_COUNT:
                out.setdefault(Path(paths[lom_scenes.SCENE_RESOURCE_BASE + number]).stem.split("_")[0], set()).add(index // WMAP_SCENE_VARIANTS)
    return out


def land_placement(saved: bytes, layout: dict, land: int, distance: int) -> dict:
    """Save description edits that place a land at a distance from home and make it the current land, as if the
    player had placed it there and come in from the world map: the cell first, the extra byte for what's left.
    A save from before home was placed (the new game's home has no cell) gets home at the corner first, as the
    opening does when the player puts it on the map."""
    home = lom_saves.land_record(saved, layout, 0)
    edits: dict = {}
    if home["x"] == LAND_CELL_NONE or home["z"] == LAND_CELL_NONE:
        home = {**home, "x": 0, "z": 0}
        edits["0"] = {"flags": home["flags"] | LAND_PLACED_FLAGS, "x": 0, "z": 0}
    last = WMAP_GRID_SIZE - 1
    dx = min(distance, max(last - home["x"], home["x"]))
    dz = min(distance - dx, max(last - home["z"], home["z"]))
    x = home["x"] + dx if home["x"] + dx <= last else home["x"] - dx
    z = home["z"] + dz if home["z"] + dz <= last else home["z"] - dz
    edits[str(land)] = {"flags": lom_saves.land_record(saved, layout, land)["flags"] | LAND_PLACED_FLAGS, "x": x, "z": z, "extra": distance - dx - dz}
    return {"lands": edits, "current_land": land}


def story_writes(disc: Path, number: int) -> dict[int, list[int]]:
    """The saved-game variables a scene's scripts write, with the offsets of the instructions that write each."""
    lom_scenes._decomp_tools()
    from tools.data.scenes.event_scripts import describe_variable

    writes: dict[int, list[int]] = {}
    for instruction in lom_scenes.scene(disc, number).scripts["instructions"]:
        written = lom_scenes.written_variable(instruction)
        if written is not None and describe_variable(written)["scope"] == "saved_game":
            writes.setdefault(written, []).append(instruction["offset"])
    return writes


def shop_opens(disc: Path, number: int) -> dict[int, list[int]]:
    """The shop lists a scene's scripts open, with the offsets of the open_shop instructions for each."""
    opens: dict[int, list[int]] = {}
    for instruction in lom_scenes.scene(disc, number).scripts["instructions"]:
        if instruction["command"] == "open_shop" and instruction["operands"]["list_index"].get("value") is not None:
            opens.setdefault(instruction["operands"]["list_index"]["value"], []).append(instruction["offset"])
    return opens


def command_uses(disc: Path, number: int) -> dict[int, list[int]]:
    """The misc_command subcommands a scene's scripts use, with the offsets of the instructions for each."""
    uses: dict[int, list[int]] = {}
    for instruction in lom_scenes.scene(disc, number).scripts["instructions"]:
        if instruction["command"] == "misc_command":
            uses.setdefault(instruction["operands"]["subcommand"]["value"], []).append(instruction["offset"])
    return uses


def command_ran(disc: Path, record_folder: Path, number: int, subcommand: int) -> bool:
    """Whether a run's coverage shows the scene running a misc_command subcommand."""
    import lom_scorecard

    path = record_folder / "coverage.txt"
    if not path.exists():
        return False
    coverage: dict[str, set[tuple[int, ...]]] = {}
    for line in path.read_text().splitlines():
        kind, *values = line.split()
        coverage.setdefault(kind, set()).add(tuple(int(v) for v in values))
    executed, _, _ = lom_scorecard.executed_instructions(disc, coverage)
    return bool(executed.get(number, set()) & set(command_uses(disc, number).get(subcommand, [])))


def shop_opened(disc: Path, record_folder: Path, number: int, shop: int) -> bool:
    """Whether a run's coverage shows the scene opening a shop list."""
    import lom_scorecard

    path = record_folder / "coverage.txt"
    if not path.exists():
        return False
    coverage: dict[str, set[tuple[int, ...]]] = {}
    for line in path.read_text().splitlines():
        kind, *values = line.split()
        coverage.setdefault(kind, set()).add(tuple(int(v) for v in values))
    executed, _, _ = lom_scorecard.executed_instructions(disc, coverage)
    return shop in lom_scorecard.covered_items(disc, {number: executed.get(number, set())})["shops"]


def guest_join_steps(guest: tuple[int, int, dict] | None) -> list[dict]:
    """The steps that bring a guest into the party first: entering a scene where they join, then the guest step.
    Without a guest, none."""
    if guest is None:
        return []
    guest_id, join_scene, join_entry = guest
    return [{"enter": {"scene": join_scene, "from": join_entry["from"], "spawn": join_entry["spawn_id"]}, "note": "a scene where this guest joins"},
            {"guest": guest_id, "note": "the script expects this guest in the party"}]


def region_steps(disc: Path, number: int, index: int) -> list[dict]:
    """The steps that set off a script region: walk to its middle (no further than its far edge), push into it
    when it spans the map, then watch its script."""
    region = next(r for r in script_regions(disc, number) if r["index"] == index)
    z_high = min(region["max_z"], REGION_FAR_EDGE)
    if region["max_z"] >= REGION_WHOLE_MAP and region["min_z"] <= 0:
        push = "right" if region["min_x"] >= REGION_MIDDLE else "left"
    elif region["max_x"] >= REGION_WHOLE_MAP and region["min_x"] <= 0:
        push = "down" if region["min_z"] >= REGION_MIDDLE else "up"
    else:
        push = None
    steps = [{"walk_to": {"x": (region["min_x"] + region["max_x"]) // 2, "z": (region["min_z"] + z_high) // 2}, "note": f"toward script region {index}"}]
    if push:
        steps.append({"press": {"buttons": [push], "frames": REGION_PUSH_FRAMES}, "note": f"into script region {index}, at the edge"})
    steps.append({"watch": EVENT_WATCH_FRAMES, "note": f"script region {index}'s script does it"})
    return steps


def marker_steps(disc: Path, number: int, index: int) -> list[dict]:
    """The step that sets off a map marker: talking at its center. It's found on the map objects the scene's exits
    are in (or object 0)."""
    objects = sorted({e["object_id"] for e in lom_scenes.change_scenes_into(disc, number)} or {0})
    marker = next(m for o in objects for m in lom_scenes.scene(disc, number).markers(o) if m.label == index)
    return [{"talk_to": {"x": marker.center_x, "z": marker.center_z}, "note": f"map marker {index}'s script does it"}]


def record_steps(kind: str, index: int, kind_name: str, follow: bool, may_leave: bool) -> list[dict]:
    """The steps that set off a layout record: watching its set-up script when entering is enough, or talking to
    it. A talk is long (it plays out) unless it opens a shop."""
    if kind == "enter":
        return [{"watch": EVENT_WATCH_FRAMES, "note": f"layout record {index}'s set-up script writes it"}]
    talk = {"record": index}
    if kind_name != "shop":
        talk["long"] = True
    if follow:
        talk["follow"] = True
    if may_leave:
        talk["event_may_leave"] = True
    return [{"talk_to": talk, "note": f"layout record {index}'s event " + ("opens it" if kind_name == "shop" else "writes it")}]


def story_route(area_name: str, name: str, number: int, reference: int, trigger: tuple[str, int, dict], entry: dict, save: str,
                guest: tuple[int, int, dict] | None = None, kind_name: str = "story", answer: int | None = None, disc: Path | None = None,
                follow: bool = False, may_leave: bool = False) -> dict:
    """A generated route that makes a scene write one saved-game variable (or, for shops, open one shop list).
    With answer, the talk answers its choices with that option (a shopkeeper's "Weapons / Armor", say). A marker
    trigger needs disc, to find where the marker is."""
    kind, index, _ = trigger
    joins = guest_join_steps(guest)
    if kind == "region":
        act = region_steps(disc, number, index)
    elif kind == "marker":
        act = marker_steps(disc, number, index)
    else:
        act = record_steps(kind, index, kind_name, follow, may_leave)
    return {
        "format": 1,
        "id": f"{area_name}-{name.lower().replace('_', '-')}-{kind_name}-{reference:x}",
        "description": f"In {name} (scene {number}), {'entering' if kind == 'enter' else f'talking to layout record {index}'} should "
                       + (f"open shop list {reference}" if kind_name == "shop" else f"run misc_command {reference:#x}" if kind_name == "command"
                          else f"write story variable {reference:#x}"),
        "start": {"save": save},
        "steps": [
            *joins,
            {"enter": {"scene": number, "from": entry["from"], "spawn": entry["spawn_id"]}, "note": "generated by lom_routes.py generate-story"},
            {"checkpoint": "entered"},
            *([{"answer": answer, "note": "the talk asks a question; this answer gets there"}] if answer is not None else []),
            *act,
            {"checkpoint": "done"},
        ],
    }


def variable_written(disc: Path, record_folder: Path, number: int, reference: int) -> bool:
    """Whether a run's coverage shows the scene writing a saved-game variable."""
    import lom_scorecard

    path = record_folder / "coverage.txt"
    if not path.exists():
        return False
    coverage: dict[str, set[tuple[int, ...]]] = {}
    for line in path.read_text().splitlines():
        kind, *values = line.split()
        coverage.setdefault(kind, set()).add(tuple(int(v) for v in values))
    executed, _, _ = lom_scorecard.executed_instructions(disc, coverage)
    return reference in lom_scorecard.covered_items(disc, {number: executed.get(number, set())})["story"]


def generate_story(args: argparse.Namespace) -> int:
    """Write a route per saved-game variable an area's scenes write (the first scene that can, by its triggers), or
    with generate-shops, per shop list they open, or with generate-commands, per misc_command subcommand they use.
    Returns how many got no route. With --only, only those variables (hex, like 0x1200), shop lists or subcommands
    (hex too)."""
    shops = args.command == "generate-shops"
    kind_name = {"generate-shops": "shop", "generate-commands": "command"}.get(args.command, "story")
    finds = {"shop": shop_opens, "command": command_uses}.get(kind_name, story_writes)
    reached = {"shop": shop_opened, "command": command_ran}.get(kind_name, variable_written)
    args.routes_out.mkdir(parents=True, exist_ok=True)
    area_numbers = {n for n, _ in area_scenes(args.area)}
    layout = lom_saves.read_layout(args.layout)
    widths = lom_saves.variable_widths(args.disc)
    base = lom_saves.load_description(args.routes_out / args.save)
    base_saved = bytes(lom_saves.saved_game(base, args.disc, layout, widths))
    wanted = {int(value, 16) for value in args.only} if args.only else None

    def save_value(reference: int) -> int:
        """A variable's value in the save the routes start from."""
        if reference == LAND_DISTANCE:
            return lom_saves.land_distance(base_saved, layout, lom_saves.current_land(base_saved, layout))
        return lom_saves.get_variable(base_saved, layout, widths, reference)

    sites: dict[int, list[tuple[int, str, list[int]]]] = {}
    for number, name in area_scenes(args.area):
        for reference, offsets in finds(args.disc, number).items():
            if wanted is None or reference in wanted:
                sites.setdefault(reference, []).append((number, name, offsets))
    failed = done = 0

    def story_save(reference: int, number: int, name: str, needs: dict, land: int | None, distance: int | None) -> str:
        """The save to try a trigger from: the routes' own, or one with the story state its path needs, and the
        area's land placed at a distance when the path tests that."""
        if not needs and distance is None:
            return args.save
        story = {
            "format": base["format"],
            "description": f"{base['description']}, with the story state {name}'s {kind_name} {reference:#x} needs"
                           + (f" (land {land} placed at distance {distance})" if distance is not None else ""),
            "source": f"built from the conditions on the path to the write in scene {number} (lom_routes.py generate-story)",
            "scene": base["scene"], **lom_saves.card_of(base),
            "variables": {**base.get("variables", {}), **{f"{r:#x}": v for r, v in needs.items()}},
            **(land_placement(base_saved, layout, land, distance) if distance is not None else {}),
        }
        story_path = args.routes_out / "saves" / f"{kind_name}-{reference:x}.json"
        story_path.parent.mkdir(exist_ok=True)
        story_path.write_text(json.dumps(story, indent=2) + "\n")
        return f"saves/{story_path.name}"

    def try_trigger(reference: int, number: int, name: str, trigger: tuple, entries: list, save: str, join) -> bool:
        """Record a trigger's route from each way in, answering choices each way a shop or a talk can, until a run
        reaches what it's for. Returns whether one did."""
        path = args.routes_out / f"{kind_name}-{reference:x}.json"
        passed = False
        for answer in [None, *([1, 2] if shops else [1])] if trigger[0] != "enter" else [None]:
            for entry in entries[:TALK_ENTRY_TRIES]:
                route = story_route(args.name, name, number, reference, trigger, entry, save, join, kind_name, answer, args.disc,
                                    follow=True)
                passed, report = record_route(path, route, args)
                folder = args.out / route["id"] / "record"
                tested = passed and reached(args.disc, folder, number, reference)
                if passed and not tested:
                    passed, report = False, f"{report}\n  the run passed, but {kind_name} {reference:#x} wasn't reached"
                print(report, flush=True)
                if passed or WALKED_INTO_SCENE_CHANGE not in report:
                    break
            if passed:
                return True
        return False

    for reference, places in sorted(sites.items()):
        recorded = False
        tries = 0
        for number, name, offsets in places:
            entries = lom_scenes.change_scenes_into(args.disc, number)
            if not entries:
                continue
            entries.sort(key=lambda e: e["from"] not in area_numbers)
            for trigger in story_triggers(args.disc, number, offsets, save_value):
                if tries >= STORY_ROUTE_TRIES:
                    break
                tries += 1
                needs = dict(trigger[2])
                guest = needs.pop(GUEST_VARIANT, NO_GUEST)
                if needs.pop(COMPANION_VARIANT, NO_GUEST) != NO_GUEST:
                    continue
                distance = needs.pop(LAND_DISTANCE, None)
                land = area_land(args.disc, args.area) if distance is not None else None
                if distance is not None and land is None:
                    continue
                save = story_save(reference, number, name, needs, land, distance)
                join = None
                if guest != NO_GUEST:
                    scenes_with = [(guest, scene, into[0]) for scene in guest_join_scenes(args.disc, guest, number, area_numbers)
                                   for into in [lom_scenes.change_scenes_into(args.disc, scene)] if into]
                    if not scenes_with:
                        continue
                    join = scenes_with[0]
                if try_trigger(reference, number, name, trigger, entries, save, join):
                    recorded = True
                    break
            if recorded or tries >= STORY_ROUTE_TRIES:
                break
        if recorded:
            done += 1
            print(f"REACHED {kind_name}-{reference:x}.json")
        else:
            failed += 1
            path = args.routes_out / f"{kind_name}-{reference:x}.json"
            for stale in (path, path.with_name(path.stem + BASELINE_SUFFIX)):
                if stale.exists():
                    stale.unlink()
            print(f"NOT GENERATED {kind_name}-{reference:x}: no trigger reached it; generator work to do")
    print(f"{done} {kind_name} route(s) recorded")
    return failed

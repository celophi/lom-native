from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import lom_saves
import lom_scenes

from .settings import ANY_SCENE, BASELINE_SUFFIX, CLEARANCES, COMPANION_VARIANT, CUTSCENE_FRAME_LIMIT, EVENT_SLOTS, EVENT_WATCH_FRAMES, GUEST_VARIANT, IN_A_BATTLE, MAX_EXPERIENCE, MENU_PRESS_FRAMES, MENU_WAIT_FRAMES, NEVER_ENDED, NO_GUEST, SAVED_GAME_KINDS, STORY_ROUTE_TRIES, TALK_ENTRY_TRIES, WORLD_MAP_SCENE
from .plans import choice_answers, companion_join_scenes, guest_join_scenes, scene_state_candidates, story_alternatives, story_paths
from .comparisons import record_route


CUTSCENE_FRAMES = 20000
ROTATE_FRAME_LIMIT = 120000
NEVER_GOT_CONTROL = "the player never got control"
NO_MARKER = "this scene has no exit marker with that label"
MOVED_ON = "an event moved the hero"
MOVED_WHILE_WALKING = "while walking, from"
LEFT_FIELD = "an event left the field"
TOOK_OTHER_EXIT = "took exit"
APPROACH_MARGIN = 14
GUEST_JOIN_TRIES = 3
COMPANION_JOIN_TRIES = 2
STORY_TRIES = 4
SCENE_SEARCH_TRIES = 12
DIRECT_ENTRY = {"from": None, "object_id": 0, "spawn_id": 0, "direct": True}
BATTLE_WAIT_FRAMES = 900
TRIGGER_REGIONS_RESOURCE = 6
BOSS_WAIT_FRAMES = 4000
LEAD_NAME_MAX = 40


def area_scenes(area: str) -> list[tuple[int, str]]:
    """The scenes whose file names start with an area prefix, like HTW for Domina."""
    out = []
    for number in range(lom_scenes.SCENE_COUNT):
        name = Path(lom_scenes.resource_paths()[lom_scenes.SCENE_RESOURCE_BASE + number]).stem
        if name.split("_")[0] == area:
            out.append((number, name))
    return out


def approach_points(disc: Path, number: int, map_object: int, label: int) -> list[tuple[int, int]]:
    """Points just outside each side of an exit's marker (below, above, left, right), to walk in from."""
    here = lom_scenes.scene(disc, number)
    for x0, y0, x1, y1, offset_x, offset_y, marker_label in lom_scenes._map_markers(disc, here.map_id(), map_object):
        if marker_label == label:
            xs, zs = (x0, x1, x0 + offset_x, x1 + offset_x), (y0, y1, y0 + offset_y, y1 + offset_y)
            cx, cz = sum(xs) // 4, sum(zs) // 4
            return [(cx, min(zs) - APPROACH_MARGIN), (cx, max(zs) + APPROACH_MARGIN), (min(xs) - APPROACH_MARGIN, cz), (max(xs) + APPROACH_MARGIN, cz)]
    return []


def enter_step(number: int, entry: dict, note: str) -> dict:
    """The enter step for a way into a scene: a change_scene from the game data, or a direct entry for a scene nothing enters."""
    if entry.get("direct"):
        return {"enter": {"scene": number, "direct": True}, "note": f"{note}; nothing in the game data enters this scene, so it's entered directly"}
    return {"enter": {"scene": number, "from": entry["from"], "spawn": entry["spawn_id"]}, "note": note}


def exit_route(area_name: str, name: str, number: int, label: int, targets: tuple[int, ...], entry: dict, save: str, watch_first: bool = False,
               strong: bool = False, guest: tuple[int, int, dict] | None = None, clearance: int = 0, approach: tuple[int, int] | None = None,
               companion: tuple[int, int, dict] | None = None, answer: int | None = None, event_may_move: bool = False) -> dict:
    """A generated route: come into a scene, walk onto one exit, checkpoint the arrival.

    With watch_first, the scene's opening event plays out before the walk. With
    strong, the hero starts at the experience cap, for exits that a battle locks
    until it's won (the runner fights it on the way).
    """
    names = ["the world map" if target == WORLD_MAP_SCENE else f"scene {target}" for target in targets]
    leads = names[0] if len(names) == 1 else "one of " + ", ".join(names)
    watch = [{"watch": EVENT_WATCH_FRAMES, "note": "the scene opens with an event"}] if watch_first else []
    if answer is not None:
        watch.append({"answer": answer, "note": "the exit's script asks a question; this answer goes on"})
    level = [{"experience": MAX_EXPERIENCE, "note": "a battle here locks the exits until it's won"}] if strong else []
    if guest is not None:
        guest_id, join_scene, join_entry = guest
        level += [{"enter": {"scene": join_scene, "from": join_entry["from"], "spawn": join_entry["spawn_id"]}, "note": "a scene where this guest joins"},
                  {"guest": guest_id, "note": "the exit's script expects this guest in the party"}]
    if companion is not None:
        template, join_scene, join_entry = companion
        level += [{"enter": {"scene": join_scene, "from": join_entry["from"], "spawn": join_entry["spawn_id"]}, "note": "a scene where a story pet joins"},
                  {"companion": template, "note": "the exit's script expects a companion in the party"}]
    return {
        "format": 1,
        "id": f"{area_name}-{name.lower().replace('_', '-')}-exit-{label}",
        "description": f"Walking onto exit {label} of {name} (scene {number}) should lead to {leads}",
        "start": {"save": save},
        "steps": [
            *level,
            enter_step(number, entry, "generated by lom_routes.py generate-exits"),
            {"checkpoint": "entered"},
            *watch,
            {"walk_to": {"exit": label, **({"clearance": clearance} if clearance else {}), **({"via": [list(approach)]} if approach else {}),
                         **({"event_may_move": True} if event_may_move else {})}},
            {"checkpoint": "arrived"},
        ],
    }


def cutscene_route(area_name: str, name: str, number: int, entry: dict, save: str) -> dict:
    """A generated route for a scene that never hands over control: record what it plays."""
    return {
        "format": 1,
        "id": f"{area_name}-{name.lower().replace('_', '-')}-cutscene",
        "description": f"Entering {name} (scene {number}) should play its event, until the player gets control or the scene stops changing",
        "start": {"save": save},
        "frame_limit": CUTSCENE_FRAME_LIMIT,
        "steps": [
            enter_step(number, entry, "generated by lom_routes.py generate-exits: the player never got control here, so its exits can't be walked"),
            {"checkpoint": "entered"},
            {"watch": CUTSCENE_FRAMES},
            {"checkpoint": "watched"},
        ],
    }


def generate_exits(args: argparse.Namespace) -> int:
    """Write one route per walk-in exit of an area's scenes. Returns how many exits got no route.

    Each exit gets the first way into its scene (by change_scene) that has its
    marker, preferring neighbouring scenes of the same area: arrivals from the
    world map start at the map's edge and expect a scripted walk in. With
    --validate, each route is recorded as it's written, and the next way in is
    tried until one works. A scene that opens with an event gets a watch before
    the walk. A scene that never hands over control gets one cutscene route
    instead of its exit routes. An exit with no working route is still written,
    as generator work to do.
    """
    args.routes_out.mkdir(parents=True, exist_ok=True)
    area_numbers = {n for n, _ in area_scenes(args.area)}
    failed = cutscene_exits = 0
    layout = lom_saves.read_layout(args.layout)
    widths = lom_saves.variable_widths(args.disc)
    base_saved = bytes(lom_saves.saved_game(lom_saves.load_description(args.routes_out / args.save), args.disc, layout, widths))

    def save_value(reference: int) -> int:
        """A variable's value in the save the routes start from."""
        return lom_saves.get_variable(base_saved, layout, widths, reference)

    def record(path: Path, route: dict) -> tuple[bool, str]:
        """Record a route as it's written and print the report."""
        passed, report = record_route(path, route, args)
        print(report, flush=True)
        return passed, report

    def try_scene_states(number: int, name: str, label: int, marker, usable: list, path: Path) -> bool:
        """Record an exit the walk can't reach from a save with one of the scene's tested variables changed."""
        base = lom_saves.load_description(args.routes_out / args.save)
        for change in scene_state_candidates(args.disc, number, save_value)[:SCENE_SEARCH_TRIES]:
            (reference, value), = change.items()
            story = {
                "format": base["format"],
                "description": f"{base['description']}, with {reference:#x} = {value} for exit {label} of {name}",
                "source": f"scene-wide search over the saved-game variables scene {number}'s scripts test (lom_routes.py generate-exits)",
                "scene": base["scene"], **lom_saves.card_of(base),
                "variables": {**base.get("variables", {}), f"{reference:#x}": value},
            }
            story_path = args.routes_out / "saves" / f"{name.lower()}-exit-{label}.json"
            story_path.parent.mkdir(exist_ok=True)
            story_path.write_text(json.dumps(story, indent=2) + "\n")
            passed, _ = record(path, exit_route(args.name, name, number, label, marker.targets, usable[0], f"saves/{story_path.name}"))
            if passed:
                print(f"SCENE STATE {path.name}: {reference:#x}={value}")
                return True
        return False

    def try_story_states(number: int, name: str, label: int, marker, usable: list, target: int, path: Path, reports: list) -> bool:
        """Record the exit from a save with the story state its script asks for, trying the alternatives in turn."""
        base = lom_saves.load_description(args.routes_out / args.save)
        strong = any(IN_A_BATTLE in report for report in reports)
        for needs in story_alternatives(args.disc, number, label, target, save_value)[:STORY_TRIES]:
            needs = dict(needs)
            guest = needs.pop(GUEST_VARIANT, NO_GUEST)
            guest = None if guest == NO_GUEST else guest
            wants_companion = needs.pop(COMPANION_VARIANT, NO_GUEST) != NO_GUEST
            if not needs and guest is None and not wants_companion:
                continue
            story = {
                "format": base["format"],
                "description": f"{base['description']}, with the story state exit {label} of {name} needs",
                "source": f"built from the conditions in scene {number}'s script {label} (lom_routes.py generate-exits)",
                "scene": base["scene"], **lom_saves.card_of(base),
                "variables": {**base.get("variables", {}), **{f"{reference:#x}": value for reference, value in needs.items()}},
            }
            story_path = args.routes_out / "saves" / f"{name.lower()}-exit-{label}.json"
            story_path.parent.mkdir(exist_ok=True)
            story_path.write_text(json.dumps(story, indent=2) + "\n")
            joins = [None]
            if guest is not None:
                joins = [(guest, scene, entries_in[0]) for scene in guest_join_scenes(args.disc, guest, number, area_numbers)
                         for entries_in in [lom_scenes.change_scenes_into(args.disc, scene)] if entries_in][:GUEST_JOIN_TRIES]
            pets = [None]
            if wants_companion:
                pets = [(template, scene, entries_in[0]) for scene, template in companion_join_scenes(args.disc, number, area_numbers)
                        for entries_in in [lom_scenes.change_scenes_into(args.disc, scene)] if entries_in][:COMPANION_JOIN_TRIES]
            for join in joins:
                for pet in pets:
                    for answer in [None, *choice_answers(args.disc, number, label)]:
                        passed, report = record(path, exit_route(args.name, name, number, label, marker.targets, usable[0], f"saves/{story_path.name}",
                                                                 strong=strong, guest=join, companion=pet, answer=answer))
                        reports.append(report)
                        if passed:
                            print(f"STORY STATE {path.name}: {', '.join(f'{r:#x}={v}' for r, v in needs.items())}"
                                  f"{f', guest {guest}' if guest is not None else ''}"
                                  f"{f', companion from scene {pet[1]}' if pet is not None else ''}{f', answer {answer}' if answer is not None else ''}")
                            return True
        return False

    def record_exit(path: Path, number: int, name: str, label: int, marker, entry: dict, **options) -> tuple[bool, str]:
        """Record an exit's route from the routes' save, with some of exit_route's options."""
        return record(path, exit_route(args.name, name, number, label, marker.targets, entry, args.save, **options))

    def record_cutscene(scene: dict, name: str, number: int, entry: dict) -> bool:
        """Record the scene's cutscene route, once per scene, and say whether it worked."""
        if scene["cutscene_recorded"] is None:
            scene["cutscene_recorded"] = record(scene["cutscene"], cutscene_route(args.name, name, number, entry, args.save))[0]
        return scene["cutscene_recorded"]

    def try_fallbacks(scene: dict, number: int, name: str, label: int, marker, usable: list, path: Path, reports: list) -> bool:
        """After every way in failed, try what the reports point at, in turn: clearances and other approaches when
        the walk took another exit or got stuck, a strong party for a battle, each answer to a choice, the story
        or scene states the exit's script needs, letting an event move the hero, and, for a scene that moves the
        hero on or never hands over control, its cutscene route instead. Returns whether one worked."""
        nonlocal cutscene_exits
        entry = usable[0]
        if any(TOOK_OTHER_EXIT in report for report in reports):
            for clearance in CLEARANCES:
                passed, report = record_exit(path, number, name, label, marker, entry, clearance=clearance)
                reports.append(report)
                if passed:
                    return True
        if any(TOOK_OTHER_EXIT in report or "stuck at" in report for report in reports):
            for point in approach_points(args.disc, number, entry["object_id"], label):
                passed, report = record_exit(path, number, name, label, marker, entry, approach=point)
                reports.append(report)
                if passed:
                    return True
        if any(IN_A_BATTLE in report for report in reports):
            passed, report = record_exit(path, number, name, label, marker, entry, strong=True)
            reports.append(report)
            if passed:
                return True
        for answer in choice_answers(args.disc, number, label):
            passed, report = record_exit(path, number, name, label, marker, entry, answer=answer)
            reports.append(report)
            if passed:
                print(f"ANSWER {path.name}: option {answer}")
                return True
        target = marker.targets[0] if len(marker.targets) == 1 else ANY_SCENE
        if try_story_states(number, name, label, marker, usable, target, path, reports):
            return True
        if any("stuck at" in report or TOOK_OTHER_EXIT in report for report in reports) and try_scene_states(number, name, label, marker, usable, path):
            return True
        if any(MOVED_WHILE_WALKING in report for report in reports):
            strong = any(IN_A_BATTLE in report for report in reports)
            passed, report = record_exit(path, number, name, label, marker, entry, strong=strong, event_may_move=True)
            reports.append(report)
            if passed:
                print(f"EVENT {path.name}: an event on the way moves the hero on; the route records where")
                return True
        if any(MOVED_ON in report or LEFT_FIELD in report for report in reports):
            path.unlink()
            if record_cutscene(scene, name, number, entry):
                cutscene_exits += 1
                print(f"CUTSCENE {path.name}: entering scene {number} moves the hero on; {scene['cutscene'].name} records what it plays")
                return True
        if any(NEVER_GOT_CONTROL in report for report in reports):
            passed, report = record_exit(path, number, name, label, marker, entry, watch_first=True)
            if passed:
                return True
            if NEVER_GOT_CONTROL in report or NO_MARKER in report or MOVED_ON in report or LEFT_FIELD in report:
                path.unlink()
                if record_cutscene(scene, name, number, entry):
                    cutscene_exits += 1
                    print(f"CUTSCENE {path.name}: scene {number} never hands over control; {scene['cutscene'].name} records what it plays")
                    return True
        return False

    for number, name in area_scenes(args.area):
        if args.only and not any(only.startswith(f"{name.lower()}-exit-") for only in args.only):
            continue
        entries = lom_scenes.change_scenes_into(args.disc, number) or [DIRECT_ENTRY]
        entries.sort(key=lambda e: e["from"] not in area_numbers)
        here = lom_scenes.scene(args.disc, number)
        markers = {}
        for entry in entries:
            for marker in here.markers(entry["object_id"]):
                if marker.targets:
                    markers.setdefault(marker.label, marker)
        scene = {"cutscene": args.routes_out / f"{name.lower()}-cutscene.json", "cutscene_recorded": None}
        for label, marker in sorted(markers.items()):
            path = args.routes_out / f"{name.lower()}-exit-{label}.json"
            if args.only and path.stem not in args.only:
                continue
            usable = [e for e in entries if any(m.label == label for m in here.markers(e["object_id"]))]
            first = exit_route(args.name, name, number, label, marker.targets, usable[0], args.save)
            if not args.validate:
                path.write_text(json.dumps(first, indent=2) + "\n")
                continue
            reports = []
            passed = False
            for entry in usable:
                passed, report = record_exit(path, number, name, label, marker, entry)
                reports.append(report)
                if passed:
                    break
            if passed or try_fallbacks(scene, number, name, label, marker, usable, path, reports):
                continue
            failed += 1
            path.write_text(json.dumps(first, indent=2) + "\n")
            print(f"NOT GENERATED {path}: no way in worked; generator work to do")
    print(f"{cutscene_exits} exit(s) in scenes that are cutscenes")
    return failed


def talk_candidates(disc: Path, number: int) -> list[tuple[int, object]]:
    """The scene's layout records with an event script (talking, touching, opening), as (index, record)."""
    lom_scenes._decomp_tools()
    from tools.data.scenes.layout import read_layout_records

    here = lom_scenes.scene(disc, number)
    return [(index, record) for index, record in enumerate(read_layout_records(here.data, here.header))
            if any(lom_scenes.event_script(record.scripts[slot]) is not None for slot in EVENT_SLOTS)
            and not placeholder_record(here, record)]


def placeholder_record(here, record) -> bool:
    """Whether every event script of a layout record is a bare return (record 0 of most scenes:
    a do-nothing template at (0, 0)), so there's nothing to talk to or test."""
    events = [event for event in (lom_scenes.event_script(record.scripts[slot]) for slot in EVENT_SLOTS) if event is not None]
    return all([i["command"] for i in here.links.resolve(entry)["instructions"]] == ["return"] for entry in events)


def record_event_ran(disc: Path, record_folder: Path, number: int, index: int) -> bool:
    """Whether a run's coverage shows one of the layout record's event scripts running."""
    import lom_scorecard

    path = record_folder / "coverage.txt"
    if not path.exists():
        return False
    coverage: dict[str, set[tuple[int, ...]]] = {}
    for line in path.read_text().splitlines():
        kind, *values = line.split()
        coverage.setdefault(kind, set()).add(tuple(int(v) for v in values))
    executed, _, _ = lom_scorecard.executed_instructions(disc, coverage)
    return (number, index) in lom_scorecard.covered_items(disc, {number: executed.get(number, set())})["records_events"]


def talk_route(area_name: str, name: str, number: int, index: int, entry: dict, save: str, watch_first: bool = False,
               event_may_leave: bool = False, strong: bool = False, long: bool = False) -> dict:
    """A generated route: come into a scene and talk to one placed actor. With strong, the hero starts at the
    experience cap, for a scene where a battle gets in the way (the talk step fights it as it walks)."""
    watch = [{"watch": EVENT_WATCH_FRAMES, "note": "the scene opens with an event"}] if watch_first else []
    level = [{"experience": MAX_EXPERIENCE, "note": "a battle here gets in the way of the talk"}] if strong else []
    return {
        "format": 1,
        "id": f"{area_name}-{name.lower().replace('_', '-')}-talk-{index}",
        "description": f"Talking to layout record {index} of {name} (scene {number}) should run its event and hand control back",
        "start": {"save": save},
        "steps": [
            *level,
            {"enter": {"scene": number, "from": entry["from"], "spawn": entry["spawn_id"]},
             "note": "generated by lom_routes.py generate-talks"},
            {"checkpoint": "entered"},
            *watch,
            {"talk_to": {"record": index, **({"event_may_leave": True} if event_may_leave else {}), **({"long": True} if long else {})},
             "note": f"layout record {index}" + ("; talking ends a quest, so the route watches it through" if event_may_leave else "")},
            {"checkpoint": "talked"},
        ],
    }


def generate_talks(args: argparse.Namespace) -> int:
    """Write one route per placed actor with an event script. Returns how many got no route.

    Each route comes into the scene the way an exit route does and talks to the
    actor from each side in turn. It only counts when the run passes and the
    coverage shows one of the actor's own event scripts running. An actor that
    only appears at some point in the story (its layout condition tests a
    saved-game variable the save doesn't meet) gets a save with that value.
    """
    lom_scenes._decomp_tools()
    from tools.data.scenes.event_scripts import describe_variable

    args.routes_out.mkdir(parents=True, exist_ok=True)
    area_numbers = {n for n, _ in area_scenes(args.area)}
    layout = lom_saves.read_layout(args.layout)
    widths = lom_saves.variable_widths(args.disc)
    base = lom_saves.load_description(args.routes_out / args.save)
    base_saved = bytes(lom_saves.saved_game(base, args.disc, layout, widths))
    failed = done = 0

    def talk_save(number: int, name: str, index: int, layout_record) -> str:
        """The save to talk to an actor from: the routes' own, or, for an actor its layout condition hides there,
        one with the condition's variable set so it's placed."""
        reference = layout_record.condition_variable_ref
        if not (reference and describe_variable(reference)["variable_kind"] < SAVED_GAME_KINDS
                and not (layout_record.condition_minimum <= lom_saves.get_variable(base_saved, layout, widths, reference)
                         <= layout_record.condition_maximum)):
            return args.save
        story = {
            "format": base["format"],
            "description": f"{base['description']}, with {reference:#x} = {layout_record.condition_minimum} "
                           f"so layout record {index} of {name} is there",
            "source": f"built from layout record {index}'s condition in scene {number} (lom_routes.py generate-talks)",
            "scene": base["scene"], **lom_saves.card_of(base),
            "variables": {**base.get("variables", {}), f"{reference:#x}": layout_record.condition_minimum},
        }
        story_path = args.routes_out / "saves" / f"{name.lower()}-talk-{index}.json"
        story_path.parent.mkdir(exist_ok=True)
        story_path.write_text(json.dumps(story, indent=2) + "\n")
        return f"saves/{story_path.name}"

    def try_talk(path: Path, number: int, name: str, index: int, entry: dict, save: str, watch_first: bool, **options) -> tuple[bool, str]:
        """Record a talk route; it only passes when one of the actor's own event scripts ran."""
        route = talk_route(args.name, name, number, index, entry, save, watch_first, **options)
        passed, report = record_route(path, route, args)
        if passed and not record_event_ran(args.disc, args.out / route["id"] / "record", number, index):
            passed, report = False, f"{report}\n  the run passed, but none of record {index}'s event scripts ran"
        print(report, flush=True)
        return passed, report

    for number, name in area_scenes(args.area):
        candidates = talk_candidates(args.disc, number)
        if args.only:
            candidates = [(i, r) for i, r in candidates if f"{name.lower()}-talk-{i}" in args.only]
        if not candidates:
            continue
        entries = lom_scenes.change_scenes_into(args.disc, number)
        if not entries:
            print(f"skip {name} (scene {number}): nothing in the game data enters it with change_scene")
            continue
        entries.sort(key=lambda e: e["from"] not in area_numbers)
        unique = list({(e["from"], e["object_id"], e["spawn_id"]): e for e in entries}.values())[:TALK_ENTRY_TRIES]
        for index, layout_record in candidates:
            path = args.routes_out / f"{name.lower()}-talk-{index}.json"
            save = talk_save(number, name, index, layout_record)
            passed = False
            for entry in unique:
                for watch_first in (False, True):
                    passed, report = try_talk(path, number, name, index, entry, save, watch_first)
                    if not passed and LEFT_FIELD in report:
                        passed, report = try_talk(path, number, name, index, entry, save, watch_first, event_may_leave=True)
                    if not passed and NEVER_ENDED in report:
                        passed, report = try_talk(path, number, name, index, entry, save, watch_first, long=True)
                    if not passed and IN_A_BATTLE in report:
                        passed, report = try_talk(path, number, name, index, entry, save, watch_first, strong=True)
                    if passed or NEVER_GOT_CONTROL not in report:
                        break
                if passed:
                    break
            if passed:
                done += 1
                print(f"TALKED {path.name}{' (story save)' if save != args.save else ''}")
            else:
                failed += 1
                baseline = path.with_name(path.stem + BASELINE_SUFFIX)
                if baseline.exists():
                    baseline.unlink()
                path.unlink()
                print(f"NOT GENERATED {name.lower()}-talk-{index}: no way in ran its event; generator work to do")
    print(f"{done} talk route(s) recorded")
    return failed


def battle_zones(disc: Path, number: int) -> list[dict]:
    """The scene's battle trigger regions, with the monsters its battle resource places."""
    lom_scenes._decomp_tools()
    from tools.data.scenes.scene_resources import read_resources

    regions, monsters = [], set()
    for resource in read_resources(lom_scenes.scene(disc, number).data, lom_scenes.scene(disc, number).header)["resources"]:
        if resource.get("resource_id") == TRIGGER_REGIONS_RESOURCE and "payload" in resource:
            regions += [region for region in resource["payload"].get("regions", []) if region["kind"] == "battle"]
        if resource.get("kind") == "battle":
            monsters |= {monster["id"] for monster in resource["battle"]["monsters"]}
    return regions if monsters else []


def monsters_fought(record_folder: Path, number: int) -> set[int]:
    """The monsters a run's coverage shows fought in one scene."""
    path = record_folder / "coverage.txt"
    fought = set()
    if path.exists():
        for line in path.read_text().splitlines():
            kind, *values = line.split()
            if kind == "monster" and int(values[0]) == number:
                fought.add(int(values[1]))
    return fought


def battle_route(area_name: str, name: str, number: int, zone: dict, entry: dict, save: str, watch_first: bool = False) -> dict:
    """A generated route: a hero at the experience cap comes into a scene, walks into one battle zone and fights."""
    watch = [{"watch": EVENT_WATCH_FRAMES, "note": "the scene opens with an event"}] if watch_first else []
    return {
        "format": 1,
        "id": f"{area_name}-{name.lower().replace('_', '-')}-battle-{zone['index']}",
        "description": f"Walking into battle zone {zone['index']} of {name} (scene {number}) should start a battle that a strong hero wins",
        "start": {"save": save},
        "steps": [
            {"experience": MAX_EXPERIENCE, "note": "strong enough to win, so the route records the whole battle"},
            {"enter": {"scene": number, "from": entry["from"], "spawn": entry["spawn_id"]},
             "note": "generated by lom_routes.py generate-battles"},
            {"checkpoint": "entered"},
            *watch,
            {"walk_to": {"x": (zone["min_x"] + zone["max_x"]) // 2, "z": -1},
             "note": f"into battle zone {zone['index']}; the walk stops when the battle starts"},
            {"fight": BATTLE_WAIT_FRAMES},
            {"checkpoint": "fought"},
        ],
    }


def generate_battles(args: argparse.Namespace) -> int:
    """Write one route per battle zone of an area. Returns how many got no route.

    A route only counts when it passes and the coverage shows a monster of the
    scene fought.
    """
    args.routes_out.mkdir(parents=True, exist_ok=True)
    area_numbers = {n for n, _ in area_scenes(args.area)}
    failed = done = 0
    for number, name in area_scenes(args.area):
        zones = battle_zones(args.disc, number)
        if args.only:
            zones = [zone for zone in zones if f"{name.lower()}-battle-{zone['index']}" in args.only]
        if not zones:
            continue
        entries = lom_scenes.change_scenes_into(args.disc, number)
        if not entries:
            print(f"skip {name} (scene {number}): nothing in the game data enters it with change_scene")
            continue
        entries.sort(key=lambda e: e["from"] not in area_numbers)
        unique = list({(e["from"], e["object_id"], e["spawn_id"]): e for e in entries}.values())[:TALK_ENTRY_TRIES]
        for zone in zones:
            path = args.routes_out / f"{name.lower()}-battle-{zone['index']}.json"
            passed = False
            for entry in unique:
                for watch_first in (False, True):
                    route = battle_route(args.name, name, number, zone, entry, args.save, watch_first)
                    if args.lead:
                        fight = next(i for i, step in enumerate(route["steps"]) if "fight" in step)
                        route["steps"][fight:fight + 1] = [step for button in args.lead.split(",")
                                                           for step in ({"press": {"buttons": button.split("+"), "frames": MENU_PRESS_FRAMES}}, {"wait": MENU_WAIT_FRAMES})]
                        route["steps"][fight:fight] = [{"wait": MENU_WAIT_FRAMES, "note": "the battle has started"}]
                        lead_name = args.lead.replace(",", "-").replace("+", "")
                        route["id"] += f"-{lead_name}" if len(lead_name) <= LEAD_NAME_MAX else f"-lead-{hashlib.sha1(args.lead.encode()).hexdigest()[:8]}"
                    passed, report = record_route(path, route, args)
                    if passed and not monsters_fought(args.out / route["id"] / "record", number):
                        passed, report = False, f"{report}\n  the run passed, but no monster was fought"
                    print(report, flush=True)
                    if passed or NEVER_GOT_CONTROL not in report:
                        break
                if passed:
                    break
            if passed:
                done += 1
                print(f"FOUGHT {path.name}: monsters {sorted(monsters_fought(args.out / route['id'] / 'record', number))}")
            else:
                failed += 1
                baseline = path.with_name(path.stem + BASELINE_SUFFIX)
                if baseline.exists():
                    baseline.unlink()
                path.unlink()
                print(f"NOT GENERATED {name.lower()}-battle-{zone['index']}: no way in started and won the battle; generator work to do")
    print(f"{done} battle route(s) recorded")
    return failed


def scene_route(area_name: str, name: str, number: int, save: str) -> dict:
    """A generated route for a scene no change_scene enters: enter it directly and let it play."""
    return {
        "format": 1,
        "id": f"{area_name}-{name.lower().replace('_', '-')}-direct",
        "description": f"Entering {name} (scene {number}) directly should load it and play its opening, until the player gets control or it stops changing",
        "start": {"save": save},
        "frame_limit": CUTSCENE_FRAME_LIMIT,
        "steps": [
            {"enter": {"scene": number, "direct": True},
             "note": "generated by lom_routes.py generate-scenes: nothing in the game data enters this scene with change_scene"},
            {"checkpoint": "entered"},
            {"watch": EVENT_WATCH_FRAMES},
            {"checkpoint": "watched"},
        ],
    }


def entry_route(area_name: str, name: str, number: int, entry: dict, save: str) -> dict:
    """A generated route for a scene with no walk-in exits: come in the way the game does and let it play."""
    route = cutscene_route(area_name, name, number, entry, save)
    route["id"] = route["id"].removesuffix("-cutscene") + "-entry"
    route["description"] = f"Entering {name} (scene {number}) should load it and play its opening, until the player gets control or it stops changing"
    route["steps"][0]["note"] = "generated by lom_routes.py generate-scenes: this scene has no walk-in exits, so no exit route enters it"
    route["steps"][2] = {"watch": EVENT_WATCH_FRAMES}
    route["frame_limit"] = CUTSCENE_FRAME_LIMIT
    return route


def has_walk_in_exits(disc: Path, number: int, entries: list[dict]) -> bool:
    """Whether any way into a scene lands in a map object with a walk-in exit."""
    here = lom_scenes.scene(disc, number)
    return any(marker.targets for entry in entries for marker in here.markers(entry["object_id"]))


def generate_scenes(args: argparse.Namespace) -> int:
    """Write a direct-entry route for each scene of an area that no change_scene enters, and an entry route for each
    scene with no walk-in exits. Returns how many failed."""
    args.routes_out.mkdir(parents=True, exist_ok=True)
    area_numbers = {n for n, _ in area_scenes(args.area)}
    failed = done = 0
    for number, name in area_scenes(args.area):
        entries = lom_scenes.change_scenes_into(args.disc, number)
        if entries and has_walk_in_exits(args.disc, number, entries):
            continue
        kind = "entry" if entries else "direct"
        if args.only and f"{name.lower()}-{kind}" not in args.only:
            continue
        path = args.routes_out / f"{name.lower()}-{kind}.json"
        entries.sort(key=lambda e: e["from"] not in area_numbers)
        route = entry_route(args.name, name, number, entries[0], args.save) if entries else scene_route(args.name, name, number, args.save)
        passed, report = record_route(path, route, args)
        print(report, flush=True)
        if passed:
            done += 1
            print(f"ENTERED {path.name}")
        else:
            failed += 1
            baseline = path.with_name(path.stem + BASELINE_SUFFIX)
            if baseline.exists():
                baseline.unlink()
            path.unlink()
            print(f"NOT GENERATED {name.lower()}-{kind}: entering it failed; generator work to do")
    print(f"{done} scene route(s) recorded")
    return failed


def boss_scenes(disc: Path, number: int) -> bool:
    """Whether a scene places monsters but has no battle trigger zone (the fight starts from an event)."""
    lom_scenes._decomp_tools()
    from tools.data.scenes.scene_resources import read_resources

    here = lom_scenes.scene(disc, number)
    monsters = any(resource.get("kind") == "battle" and resource["battle"]["monsters"] for resource in read_resources(here.data, here.header)["resources"])
    return monsters and not battle_zones(disc, number)


def script_regions(disc: Path, number: int) -> list[dict]:
    """The scene's script trigger regions (resource 6 regions that run a script when the hero walks in)."""
    lom_scenes._decomp_tools()
    from tools.data.scenes.scene_resources import read_resources

    here = lom_scenes.scene(disc, number)
    return [region for resource in read_resources(here.data, here.header)["resources"]
            if resource.get("resource_id") == TRIGGER_REGIONS_RESOURCE and "payload" in resource
            for region in resource["payload"].get("regions", []) if region["kind"] == "script"]


def battle_starts(disc: Path, number: int) -> list[int]:
    """The offsets of the scene's start_battle instructions."""
    return [instruction["offset"] for instruction in lom_scenes.scene(disc, number).scripts["instructions"] if instruction["command"] == "start_battle"]


def boss_route(area_name: str, name: str, number: int, entry: dict, save: str, trigger: list[dict] | None = None,
               rotate: bool = False, fight_for: int = 0) -> dict:
    """A generated route: a level-99 hero comes into a boss room, sets its battle off (by coming in, or with the
    trigger steps: walking into a region, talking to someone), wins, and watches what follows."""
    return {
        "format": 1,
        "id": f"{area_name}-{name.lower().replace('_', '-')}-boss",
        "description": f"Entering {name} (scene {number}) should start its battle, which a strong hero wins, then play what follows",
        "start": {"save": save},
        "frame_limit": ROTATE_FRAME_LIMIT if rotate else CUTSCENE_FRAME_LIMIT,
        "steps": [
            {"experience": MAX_EXPERIENCE, "note": "strong enough to win, so the route records the whole battle"},
            {"enter": {"scene": number, "from": entry["from"], "spawn": entry["spawn_id"]}, "note": "generated by lom_routes.py generate-bosses"},
            {"checkpoint": "entered"},
            *(trigger or []),
            {"fight": BOSS_WAIT_FRAMES, "confirm_text": True, **({"rotate_targets": True} if rotate else {}),
             **({"fight_for": fight_for} if fight_for else {}),
             "note": "the battle starts from the room's event, not a zone"},
            {"checkpoint": "fought"},
            {"watch": EVENT_WATCH_FRAMES},
            {"checkpoint": "after"},
        ],
    }


def generate_bosses(args: argparse.Namespace) -> int:
    """Write one boss route per scene of an area whose battle has no trigger zone. Returns how many got no route."""
    args.routes_out.mkdir(parents=True, exist_ok=True)
    area_numbers = {n for n, _ in area_scenes(args.area)}
    layout = lom_saves.read_layout(args.layout)
    widths = lom_saves.variable_widths(args.disc)
    base = lom_saves.load_description(args.routes_out / args.save)
    base_saved = bytes(lom_saves.saved_game(base, args.disc, layout, widths))

    def save_value(reference: int) -> int:
        """A variable's value in the save the routes start from."""
        return lom_saves.get_variable(base_saved, layout, widths, reference)

    failed = done = 0
    for number, name in area_scenes(args.area):
        if not boss_scenes(args.disc, number) or (args.only and f"{name.lower()}-boss" not in args.only):
            continue
        entries = lom_scenes.change_scenes_into(args.disc, number)
        if not entries:
            print(f"skip {name} (scene {number}): nothing in the game data enters it with change_scene")
            continue
        entries.sort(key=lambda e: e["from"] not in area_numbers)
        path = args.routes_out / f"{name.lower()}-boss.json"
        tries = [(entry, args.save, None) for entry in list({(e["from"], e["object_id"], e["spawn_id"]): e for e in entries}.values())[:TALK_ENTRY_TRIES]]
        tries += [(entries[0], args.save, [{"walk_to": {"x": (r["min_x"] + r["max_x"]) // 2, "z": -1}, "note": f"into script region {r['index']}"}])
                  for r in script_regions(args.disc, number)]
        for kind, index, needs in story_triggers(args.disc, number, battle_starts(args.disc, number), save_value)[:STORY_ROUTE_TRIES]:
            needs = dict(needs)
            if needs.pop(GUEST_VARIANT, NO_GUEST) != NO_GUEST or needs.pop(COMPANION_VARIANT, NO_GUEST) != NO_GUEST:
                continue
            save = args.save
            if needs:
                story = {"format": base["format"], "description": f"{base['description']}, with the story state {name}'s battle needs",
                         "source": f"built from the conditions on the path to start_battle in scene {number} (lom_routes.py generate-bosses)",
                         "scene": base["scene"], **lom_saves.card_of(base),
                         "variables": {**base.get("variables", {}), **{f"{r:#x}": v for r, v in needs.items()}}}
                story_path = args.routes_out / "saves" / f"{name.lower()}-boss.json"
                story_path.parent.mkdir(exist_ok=True)
                story_path.write_text(json.dumps(story, indent=2) + "\n")
                save = f"saves/{story_path.name}"
            talk = [{"talk_to": {"record": index, "long": True}, "note": f"layout record {index} starts the battle"}]
            tries.append((entries[0], save, None if kind == "enter" else talk))
        passed = False
        for entry, save, trigger in tries:
            route = boss_route(args.name, name, number, entry, save, trigger, rotate=args.rotate_targets, fight_for=args.fight_for)
            passed, report = record_route(path, route, args)
            if passed and not monsters_fought(args.out / route["id"] / "record", number):
                passed, report = False, f"{report}\n  the run passed, but no monster was fought"
            print(report, flush=True)
            if passed:
                break
        if passed:
            done += 1
            print(f"FOUGHT {path.name}: monsters {sorted(monsters_fought(args.out / route['id'] / 'record', number))}")
        else:
            failed += 1
            for stale in (path, path.with_name(path.stem + BASELINE_SUFFIX)):
                if stale.exists():
                    stale.unlink()
            print(f"NOT GENERATED {name.lower()}-boss: no way in started and won the battle; generator work to do")
    print(f"{done} boss route(s) recorded")
    return failed


def story_triggers(disc: Path, number: int, offsets: list[int], default) -> list[tuple[str, int, dict]]:
    """Ways to make the scene run one of the write instructions, cheapest first: ("enter", record, needs) for a
    layout record's set-up scripts, which run when the scene starts, and ("talk", record, needs) for its events."""
    lom_scenes._decomp_tools()
    from tools.data.scenes.layout import read_layout_records

    from tools.data.scenes.event_scripts import describe_variable

    here = lom_scenes.scene(disc, number)
    entries = {index: entry.get("target_offset") for index, entry in enumerate(here.scripts["entries"])}
    goal_offsets = set(offsets)
    found = []
    for index, record in enumerate(read_layout_records(here.data, here.header)):
        condition = {}
        reference = record.condition_variable_ref
        if (reference and describe_variable(reference)["scope"] == "saved_game"
                and not record.condition_minimum <= default(reference) <= record.condition_maximum):
            condition = {reference: record.condition_minimum}
        for slot, script in enumerate(record.scripts):
            event = lom_scenes.event_script(script)
            if event is None or entries.get(event) is None:
                continue
            if slot in EVENT_SLOTS and not (record.enabled_events >> slot) & 1:
                continue
            kind = "enter" if slot >= len(EVENT_SLOTS) else "talk"
            for needs in story_paths(disc, number, entries[event], lambda instruction: instruction["offset"] in goal_offsets, default)[:2]:
                found.append((kind, index, {**condition, **needs}))
    objects = {entry["object_id"] for entry in lom_scenes.change_scenes_into(disc, number)} or {0}
    for map_object in sorted(objects):
        for marker in here.markers(map_object):
            if marker.targets or entries.get(marker.label) is None:
                continue
            for needs in story_paths(disc, number, entries[marker.label], lambda instruction: instruction["offset"] in goal_offsets, default)[:2]:
                found.append(("marker", marker.label, needs))
    for region in script_regions(disc, number):
        if entries.get(region["target"]) is None:
            continue
        for needs in story_paths(disc, number, entries[region["target"]], lambda instruction: instruction["offset"] in goal_offsets, default)[:2]:
            found.append(("region", region["index"], needs))
    unique = {(kind, index, frozenset(needs.items())): (kind, index, needs) for kind, index, needs in found}
    return sorted(unique.values(), key=lambda t: (len(t[2]), t[0] != "enter", t[1]))

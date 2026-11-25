from __future__ import annotations

import argparse
import collections
import json
import os
import re
from pathlib import Path

import lom_saves
import lom_scenes

from .settings import BASELINE_SUFFIX, BOUNDARY_BASELINE, CHANNELS, CLEARANCES, CUTSCENE_FRAME_LIMIT, EVENT_WATCH_FRAMES, ROUTES_ROOT, WALKED_INTO_SCENE_CHANGE
from .plans import RouteError
from .records import change_differences, describe_changes, frame_fields, read_record
from .cases import Cases, expand_routes, run_case, run_route


GPU_CALLS = {0xA0: "texture upload (x y w h)", 0xC0: "its pixels were read back from VRAM this frame, so they aren't hashed", 0x02: "clear (x y w h)", 0x80: "VRAM move (x y w h)", 0xE1: "texture page",
             0xE2: "texture window", 0xE3: "drawing area start", 0xE4: "drawing area end", 0xE5: "drawing offset", 0xE6: "mask bits"}
GPU_DUMP_VARIABLE = "LOM_ROUTE_GPU_DUMP"


def gpu_command(line: str) -> str:
    """A line of a GPU dump in plain terms: a primitive's kind and command byte, a drawing call by name (the
    recorder's tags), or a texture upload's pixels."""
    words = line.split()
    if words[:1] == ["call"] and len(words) > 1:
        return f"{GPU_CALLS.get(int(words[1]), 'call ' + words[1])}: {' '.join(words[2:])}"
    if words[:1] == ["pixels"]:
        return f"the uploaded texture's pixels, hash {words[1]}"
    if words[:1] != ["prim"] or len(words) < 2:
        return line
    command = int(words[1], 16) >> 24
    kind = ("polygon" if command < 0x40 else "line" if command < 0x60 else "rectangle") if 0x20 <= command < 0x80 else "command"
    return f"{kind} {command:#04x}: {' '.join(words[1:])}"


def compare_builds(args: argparse.Namespace) -> int:
    """Run one route on two builds, recording everything, and print what differs: the end and notes, the change
    lists at each checkpoint, then the first frame that differs and in which channels, and for a picture the first
    GPU call of that frame that differs. Returns 0 when the runs match, 1 when they differ."""
    sides = {"A": (args.runner, args.disc), "B": (args.against, args.disc_b or args.disc)}
    runs = {}
    for side, (runner, disc) in sides.items():
        side_args = argparse.Namespace(**{**vars(args), "runner": runner, "disc": disc, "out": args.out / side, "baselines": args.out / side / "baselines"})
        case = Cases(side_args).case(str(args.route))
        passed, report = run_case(case, side_args, "record")
        print(f"{side}: {runner}\n  {report.splitlines()[0]}")
        runs[side] = (read_record(args.out / side / case.id / "record"), args.out / side / case.id, case, side_args)
    (a, a_folder, case, _), (b, b_folder, _, _) = runs["A"], runs["B"]
    layout = lom_saves.read_layout(args.layout)
    start = lom_saves.build(case.save, args.disc, layout)[layout["saved_game_offset"] : layout["saved_game_offset"] + layout["saved_game_bytes"]]
    differences = []
    if a["end"] != b["end"]:
        differences.append(f"the end: A {a['end']}, B {b['end']}")
    if a["notes"] != b["notes"]:
        differences.append("the notes:\n    A " + "; ".join(a["notes"]) + "\n    B " + "; ".join(b["notes"]))
    changed = change_differences(describe_changes(a, a_folder / "record", start, layout), describe_changes(b, b_folder / "record", start, layout))
    differences += [line.replace("baseline only", "A only").replace("this run only", "B only") for line in changed]
    index = next((i for i, (x, y) in enumerate(zip(a["frames"], b["frames"])) if x != y), None)
    first = (a["frames"][index], b["frames"][index]) if index is not None else None
    if len(a["frames"]) != len(b["frames"]):
        differences.append(f"A has {len(a['frames'])} frames, B {len(b['frames'])}")
    if first:
        want, got = frame_fields(first[0]), frame_fields(first[1])
        channels = [channel for channel in CHANNELS if want.get(channel) != got.get(channel)]
        differences.append(f"frame {want['frame']} is the first to differ, in {', '.join(channels)}"
                           + "".join(f"\n    {channel}: A {want.get(channel)}, B {got.get(channel)}" for channel in channels if channel != "picture"))
        if "picture" in channels:
            dumps = []
            for side, (runner, disc) in sides.items():
                occurrence = sum(frame_fields(line)["frame"] == want["frame"] for line in a["frames"][:index])
                os.environ[GPU_DUMP_VARIABLE] = f"{want['frame']}@{occurrence}"
                side_args = runs[side][3]
                side_args.out = args.out / side / "picture"
                run_case(Cases(side_args).case(str(args.route)), side_args, "record")
                dumps.append((side_args.out / case.id / "record" / f"gpu-{want['frame']}.txt").read_text().splitlines())
            del os.environ[GPU_DUMP_VARIABLE]
            index = next((i for i, (x, y) in enumerate(zip(*dumps)) if x != y), min(map(len, dumps)))
            differences.append(f"  the picture's GPU calls in that frame: {len(dumps[0])} in A, {len(dumps[1])} in B; call {index} is the first to differ"
                               + (f", after {gpu_command(dumps[0][index - 1])}" if 0 < index <= len(dumps[0]) else "")
                               + "".join(f"\n    {side}: {gpu_command(dump[index]) if index < len(dump) else '(none)'}" for side, dump in zip("AB", dumps)))
    print("\n".join(differences) if differences else "no difference: the two runs match on every frame and checkpoint")
    print(f"both runs' records are in {args.out}/A and {args.out}/B (frames.txt, save-<n>.bin)")
    return 1 if differences else 0


WIDESCREEN_VARIABLE = "LOM_ROUTE_WIDESCREEN"
CAPTURE_VARIABLE = "LOM_ROUTE_CAPTURE"
WIDESCREEN_LOG = re.compile(r"\[lom-native\] FIELD widescreen: scene (\d+), map (-?\d+) px: ([a-z ]+)")
ORIGINAL_WIDTH, WIDE_WIDTH = 320, 426
WIDE_MARGIN = (WIDE_WIDTH - ORIGINAL_WIDTH) // 2
WIDESCREEN_BLOCK = 8
WIDESCREEN_BLOCK_SHIFT = WIDE_MARGIN * 24 // 16
WIDESCREEN_BLOCK_MATCH = 0.65
WIDESCREEN_DITHER = 9


def read_ppm(path: Path) -> tuple[int, int, bytes] | None:
    """A binary PPM the runner wrote (P6, 255): width, height and RGB bytes, or None if it isn't there."""
    if not path.exists():
        return None
    data = path.read_bytes()
    words, at = [], 0
    while len(words) < 4:
        while data[at : at + 1].isspace():
            at += 1
        start = at
        while not data[at : at + 1].isspace():
            at += 1
        words.append(data[start:at])
    width, height = int(words[1]), int(words[2])
    return width, height, data[at + 1 : at + 1 + width * height * 3]


def middle_match(narrow: tuple[int, int, bytes], wide: tuple[int, int, bytes]) -> tuple[int, float]:
    """Where the 4:3 picture sits in the wide one: the draw camera's shift (-53 to 53, 0 when the camera isn't near
    a map end) whose 320 columns match it best, and the share of its pixels that match there."""
    width, height, pixels = narrow
    wide_width, _, wide_pixels = wide
    rows = [pixels[y * width * 3 : (y + 1) * width * 3] for y in range(height)]
    best = (0, -1)
    for shift in range(-WIDE_MARGIN, WIDE_MARGIN + 1):
        left = (WIDE_MARGIN + shift) * 3
        same = sum(row == wide_pixels[y * wide_width * 3 + left : y * wide_width * 3 + left + width * 3] for y, row in enumerate(rows))
        if same > best[1]:
            best = (shift, same)
    shift = best[0]
    left = (WIDE_MARGIN + shift) * 3
    matching = 0
    for y, row in enumerate(rows):
        other = wide_pixels[y * wide_width * 3 + left : y * wide_width * 3 + left + width * 3]
        matching += sum(row[x : x + 3] == other[x : x + 3] for x in range(0, width * 3, 3))
    return shift, matching / (width * height)


def blocks_found(narrow: tuple[int, int, bytes], wide: tuple[int, int, bytes]) -> float:
    """The share of the 4:3 picture's 8x8 blocks found in the wide picture in the same rows, each at the shift (up to
    WIDESCREEN_BLOCK_SHIFT either way, and inside the wide picture) where it matches, to within WIDESCREEN_DITHER."""
    width, height, pixels = narrow
    wide_width, _, wide_pixels = wide
    found = total = 0
    span = WIDESCREEN_BLOCK * 3

    def close(row: bytes, other: bytes) -> bool:
        """Check two rows of pixels match, allowing for the widescreen view's dithering.

        @param row One row.
        @param other The other.
        @return Whether they match.
        """
        return len(row) == len(other) and all(abs(a - b) <= WIDESCREEN_DITHER for a, b in zip(row, other))

    for top in range(0, height - WIDESCREEN_BLOCK + 1, WIDESCREEN_BLOCK):
        for x in range(0, width - WIDESCREEN_BLOCK + 1, WIDESCREEN_BLOCK):
            block = [pixels[(top + y) * width * 3 + x * 3 : (top + y) * width * 3 + x * 3 + span] for y in range(WIDESCREEN_BLOCK)]
            total += 1
            shifts = [shift for shift in range(-WIDESCREEN_BLOCK_SHIFT, WIDESCREEN_BLOCK_SHIFT + 1)
                      if 0 <= WIDE_MARGIN + shift + x <= wide_width - WIDESCREEN_BLOCK]
            for same in (bytes.__eq__, close):
                if any(all(same(row, wide_pixels[(top + y) * wide_width * 3 + (WIDE_MARGIN + shift + x) * 3 :
                                                 (top + y) * wide_width * 3 + (WIDE_MARGIN + shift + x) * 3 + span])
                           for y, row in enumerate(block)) for shift in shifts):
                    found += 1
                    break
    return found / total if total else 1.0


def widescreen_settings(plan_text: str, start_on: bool) -> list[bool]:
    """Whether the setting is on at each checkpoint of a plan, starting from start_on (the plan can turn it over)."""
    on, settings = start_on, []
    for line in plan_text.splitlines():
        words = line.split()
        if words[:1] == ["widescreen"]:
            on = words[1] != "0"
        elif words[:1] == ["checkpoint"]:
            settings.append(on)
    return settings


def wide_scenes(log: Path) -> dict[int, str]:
    """What the game decided for each room it logged while the setting was on (the last decision for each)."""
    decisions = {}
    for match in WIDESCREEN_LOG.finditer(log.read_text(errors="replace") if log.exists() else ""):
        decisions[int(match.group(1))] = match.group(3)
    return decisions


def record_widescreen_side(name: str, args: argparse.Namespace, side: str, on: bool) -> tuple:
    """Record a route with widescreen off or on, saving the screen at each checkpoint.

    Returns (passed, report, record, folder, plan, on)."""
    side_args = argparse.Namespace(**{**vars(args), "out": args.out / side, "baselines": args.out / side / "baselines"})
    case = Cases(side_args).case(name)
    saved = {key: os.environ.get(key) for key in (WIDESCREEN_VARIABLE, CAPTURE_VARIABLE)}
    os.environ[CAPTURE_VARIABLE] = "1"
    if on:
        os.environ[WIDESCREEN_VARIABLE] = "1"
    else:
        os.environ.pop(WIDESCREEN_VARIABLE, None)
    try:
        passed, report = run_case(case, side_args, "record")
    finally:
        for key, value in saved.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value
    folder = args.out / side / case.id
    return passed, report, read_record(folder / "record"), folder, (folder / "plan.txt").read_text(), on


def widescreen_play_problems(off: dict, on: dict) -> list[str]:
    """What differs between the two runs apart from the picture: the end, the notes, the checkpoints and the
    game state at them, and each frame's other channels."""
    problems = []
    if off["end"] != on["end"]:
        problems.append(f"the end differs: off {off['end']}, on {on['end']}")
    if off["notes"] != on["notes"]:
        problems.append("the notes differ:\n    off " + "; ".join(off["notes"]) + "\n    on  " + "; ".join(on["notes"]))
    if off["checkpoints"] != on["checkpoints"] or off["states"] != on["states"]:
        problems.append("the checkpoints or the game state at them differ")
    if len(off["frames"]) != len(on["frames"]):
        problems.append(f"off has {len(off['frames'])} frames, on {len(on['frames'])}")
    for a, b in zip(off["frames"], on["frames"]):
        want, got = frame_fields(a), frame_fields(b)
        channels = [channel for channel in ("frame", *CHANNELS) if channel != "picture" and want.get(channel) != got.get(channel)]
        if channels:
            problems.append(f"frame {want['frame']} is the first to differ, in {', '.join(channels)}: off {a}, on {b}")
            break
    return problems


def widescreen_picture_problems(off: dict, off_folder: Path, off_plan: str, on_folder: Path, on_plan: str) -> tuple[list[str], list[str]]:
    """Check each checkpoint's pictures: wide exactly when the setting was on and the game drew the room wide, the
    4:3 one inside a wide one, and the same in both runs when neither is wide. Returns (lines, problems)."""
    lines, problems = [], []
    decided = {side: wide_scenes(folder / "runner.log") for side, folder in (("off", off_folder), ("on", on_folder))}
    settings = {"off": widescreen_settings(off_plan, False), "on": widescreen_settings(on_plan, True)}
    for index, checkpoint in enumerate(off["checkpoints"]):
        label, scene = checkpoint.split()[0], int(frame_fields(checkpoint).get("scene", -1))
        pictures = {side: read_ppm(folder / "record" / f"capture-{index}.ppm") for side, folder in (("off", off_folder), ("on", on_folder))}
        if None in pictures.values():
            problems.append(f"{label}: a capture is missing")
            continue
        for side in pictures:
            wide = index < len(settings[side]) and settings[side][index] and decided[side].get(scene) == "wide"
            width = pictures[side][0]
            if width != (WIDE_WIDTH if wide else ORIGINAL_WIDTH):
                problems.append(f"{label} (scene {scene}): the {side} picture is {width} wide, but the room was "
                                f"{decided[side].get(scene, 'not logged')} with the setting {'on' if settings[side][index] else 'off'}")
        narrow, wide = pictures["off"], pictures["on"]
        if narrow[0] == wide[0]:
            same = narrow == wide
            lines.append(f"  {label} (scene {scene}): {narrow[0]} wide in both, {'the same' if same else 'DIFFERENT'}")
            if not same:
                problems.append(f"{label}: neither picture is widened, but they differ")
        elif narrow[0] == ORIGINAL_WIDTH and wide[0] == WIDE_WIDTH and narrow[1] == wide[1]:
            shift, share = middle_match(narrow, wide)
            blocks = blocks_found(narrow, wide)
            lines.append(f"  {label} (scene {scene}): wide; {blocks:.1%} of the 4:3 picture's blocks found in it; "
                         f"at the best single shift ({shift:+d}) {share:.1%} of its pixels match")
            if blocks < WIDESCREEN_BLOCK_MATCH:
                problems.append(f"{label}: only {blocks:.1%} of the 4:3 picture's blocks are in the wide picture")
    return lines, problems


def compare_widescreen(args: argparse.Namespace) -> int:
    """Run each route with widescreen off and on and check, from the outside, that only the picture changes and that
    the picture is right: everything but the picture matches frame by frame; each checkpoint's capture is wide
    exactly when the setting was on and the game drew the room wide; a wide capture's middle is the 4:3 one at the
    draw camera's shift; and a capture that isn't wide is the same in both runs. Returns 0 when every route passes."""
    names = []
    for path in args.routes:
        names += (sorted(str(f) for f in path.glob("*.json") if not f.name.endswith(BASELINE_SUFFIX)) if path.is_dir() else expand_routes([path]))
    if not names:
        print("widescreen: no routes found (folders aren't searched below their own level)", flush=True)
        return 1
    failures = 0
    for name in names:
        sides = {side: record_widescreen_side(name, args, side, on) for side, on in (("off", False), ("on", True))}
        problems = []
        for side, (passed, report, *_rest) in sides.items():
            if not passed:
                problems.append(f"the {side} run didn't finish: {report.splitlines()[-1].strip()}")
        (_, _, off, off_folder, off_plan, _), (_, _, on, on_folder, on_plan, _) = sides["off"], sides["on"]
        problems += widescreen_play_problems(off, on)
        lines, picture_problems = widescreen_picture_problems(off, off_folder, off_plan, on_folder, on_plan)
        problems += picture_problems
        case_id = off_folder.name
        print(f"{'PASS' if not problems else 'FAIL'} {case_id}")
        print("\n".join(lines + [f"  problem: {problem}" for problem in problems]), flush=True)
        failures += bool(problems)
    print(f"\nwidescreen: {len(names) - failures} of {len(names)} routes pass; "
          f"runs and captures (capture-<n>.ppm in each record folder) are in {args.out}/off and {args.out}/on")
    return 1 if failures else 0


REMASTER_VARIABLE = "LOM_ROUTE_REMASTER"
REMASTER_USED_LOG = re.compile(r"\[lom-native\] The Remaster's ([a-z0-9_A]+(?:\.psb)?)[.,]")


def compare_remaster(args: argparse.Namespace) -> int:
    """Run each route without and with the Remaster (HD backgrounds and pictures, and the soundtrack --music picks) and
    check that the game plays the same: the scene, the hero and the save match frame by frame, and the checkpoints come at the
    same frames. The picture and sound change, and so can the game state hash: the Remaster's maps have bigger layers,
    and some script variables pick that up. Those are listed, not failed. A few of its rooms move walls to fit the new
    art, so a route through one of those can rightly differ; the report lists the rooms and recordings each run used.
    Returns 0 when every route passes."""
    names = []
    for path in args.routes:
        names += (sorted(str(f) for f in path.glob("*.json") if not f.name.endswith(BASELINE_SUFFIX)) if path.is_dir() else expand_routes([path]))
    if not names:
        print("remaster: no routes found (folders aren't searched below their own level)", flush=True)
        return 1
    failures = 0
    settings = {REMASTER_VARIABLE: str(args.remaster), "LOM_REMASTER_HD": "1", "LOM_REMASTER_MUSIC": str(args.music)}
    for name in names:
        sides = {}
        for side, on in (("off", False), ("on", True)):
            side_args = argparse.Namespace(**{**vars(args), "out": args.out / side, "baselines": args.out / side / "baselines"})
            case = Cases(side_args).case(name)
            saved = {key: os.environ.get(key) for key in (*settings, CAPTURE_VARIABLE)}
            os.environ[CAPTURE_VARIABLE] = "1"
            for key, value in settings.items():
                if on:
                    os.environ[key] = value
                else:
                    os.environ.pop(key, None)
            try:
                passed, report = run_case(case, side_args, "record")
            finally:
                for key, value in saved.items():
                    if value is None:
                        os.environ.pop(key, None)
                    else:
                        os.environ[key] = value
            folder = args.out / side / case.id
            sides[side] = (passed, report, read_record(folder / "record"), folder)
        problems = []
        for side, (passed, report, *_rest) in sides.items():
            if not passed:
                problems.append(f"the {side} run didn't finish: {report.splitlines()[-1].strip()}")
        (_, _, off, _), (_, _, on, on_folder) = sides["off"], sides["on"]
        used = []
        log = on_folder / "runner.log"
        for line in (log.read_text(errors="replace").splitlines() if log.exists() else []):
            match = REMASTER_USED_LOG.search(line)
            if match:
                used.append(match.group(1))
        if off["end"] != on["end"]:
            problems.append(f"the end differs: off {off['end']}, on {on['end']}")
        if off["notes"] != on["notes"]:
            problems.append("the notes differ:\n    off " + "; ".join(off["notes"]) + "\n    on  " + "; ".join(on["notes"]))
        played = ("frame", "scene", "hero", "save")
        if [{k: frame_fields(c).get(k) for k in played} for c in off["checkpoints"]] != [
                {k: frame_fields(c).get(k) for k in played} for c in on["checkpoints"]]:
            problems.append("the checkpoints differ")
        if len(off["frames"]) != len(on["frames"]):
            problems.append(f"off has {len(off['frames'])} frames, on {len(on['frames'])}")
        notes = []
        for a, b in zip(off["frames"], on["frames"]):
            want, got = frame_fields(a), frame_fields(b)
            channels = [channel for channel in ("frame", *CHANNELS) if channel not in ("picture", "sound") and want.get(channel) != got.get(channel)]
            if channels and set(channels) != {"state"}:
                problems.append(f"frame {want['frame']} is the first to differ, in {', '.join(channels)}: off {a}, on {b}")
                break
            if channels and not notes:
                notes.append(f"the game state hash first differs at frame {want['frame']}")
        changed = [f"{x} / {y}" for x, y in zip(off["states"], on["states"]) if x != y]
        if changed:
            notes.append("the state at checkpoints: " + "; ".join(changed[:2]) + (" ..." if len(changed) > 2 else ""))
        print(f"{'PASS' if not problems else 'FAIL'} {case.id}")
        print("\n".join([f"  used: {', '.join(used) or 'nothing'}"] + [f"  note: {note}" for note in notes] +
                        [f"  problem: {problem}" for problem in problems]), flush=True)
        failures += bool(problems)
    print(f"\nremaster: {len(names) - failures} of {len(names)} routes pass; runs and captures are in {args.out}/off and {args.out}/on")
    return 1 if failures else 0


SWEEP_SAVE = ROUTES_ROOT / "handwritten" / "saves" / "home-bedroom-new-game.json"
SWEEP_SETTLE_FRAMES = 60
BATTLE_ENTRY_FLAG = 0x8000
FIELD_DRAW_ROWS = range(8, 232)


def compare_boundaries(args: argparse.Namespace) -> int:
    """Run each route and compare what crossed the platform boundaries with the stored baseline, or record it with
    --record. A difference means the game asked the platform for something else, even where the picture and sound
    came out the same. Returns 0 when every route matches."""
    names = []
    for path in args.routes:
        names += (sorted(str(f) for f in path.rglob("*.json") if not f.name.endswith(BASELINE_SUFFIX) and "saves" not in f.relative_to(path).parts)
                  if path.is_dir() else expand_routes([path]))
    if not names:
        print("boundaries: no routes found", flush=True)
        return 1
    stored = json.loads(BOUNDARY_BASELINE.read_text()) if BOUNDARY_BASELINE.exists() else {}
    run_args = argparse.Namespace(**{**vars(args), "baselines": args.out / "baselines"})
    failures = 0
    recorded = {}
    for name in names:
        case = Cases(run_args).case(name)
        passed, report = run_case(case, run_args, "record")
        boundary = args.out / case.id / "record" / "boundary.txt"
        lines = [line for line in boundary.read_text().splitlines() if not line.startswith("event ")] if boundary.exists() else []
        if not passed:
            failures += 1
            print(f"FAIL {case.id}: the route didn't finish: {report.splitlines()[-1].strip() if report else ''}", flush=True)
            continue
        if args.record:
            recorded[case.id] = lines
            print(f"RECORDED {case.id}: {len(lines)} lines", flush=True)
            continue
        expected = stored.get(case.id)
        if expected is None:
            failures += 1
            print(f"FAIL {case.id}: no boundary baseline; record one with `lom_routes.py boundaries --record`", flush=True)
        elif expected != lines:
            failures += 1
            first = next((i for i, (a, b) in enumerate(zip(expected, lines)) if a != b), min(len(expected), len(lines)))
            print(f"FAIL {case.id}: line {first + 1} differs:\n  baseline {expected[first] if first < len(expected) else '(none)'}\n"
                  f"  run      {lines[first] if first < len(lines) else '(none)'}\n"
                  f"  rerun with LOM_ROUTE_BOUNDARY_LOG=1 for every event in {boundary}", flush=True)
        else:
            print(f"PASS {case.id}", flush=True)
    if args.record:
        stored.update(recorded)
        BOUNDARY_BASELINE.write_text(json.dumps(dict(sorted(stored.items())), indent=1) + "\n")
        print(f"boundaries: recorded {len(recorded)} routes in {BOUNDARY_BASELINE}", flush=True)
    print(f"boundaries: {len(names) - failures} of {len(names)} routes {'recorded' if args.record else 'match'}", flush=True)
    return 1 if failures else 0


def sweep_route(number: int, entries: list[dict]) -> dict:
    """A route that enters one room as the game does (by a battle entry when there is one, since most fights happen
    in rooms entered that way), lets it settle, and stops at a checkpoint."""
    battle = [entry for entry in entries if entry["scene_id"] & BATTLE_ENTRY_FLAG]
    entry = (battle or entries or [None])[0]
    enter = {"scene": number, "direct": True} if entry is None else {"scene": number, "from": entry["from"], "spawn": entry["spawn_id"]}
    return {
        "format": 1,
        "id": f"widescreen-sweep-{number}",
        "description": f"Scene {number} should be drawn wide when its shape allows, with nothing missing at the sides",
        "start": {"save": str(SWEEP_SAVE)},
        "frame_limit": CUTSCENE_FRAME_LIMIT,
        "steps": [{"enter": enter}, {"watch": EVENT_WATCH_FRAMES}, {"wait": SWEEP_SETTLE_FRAMES}, {"checkpoint": "settled"}],
    }


def black_side_pixels(picture: tuple[int, int, bytes]) -> int:
    """Pure black pixels in a wide picture's side strips, inside the rows the field draws in: where nothing was drawn.
    Some rooms are drawn over black on purpose, so this is a number to look at, not a failure."""
    width, _, pixels = picture
    black = 0
    for y in FIELD_DRAW_ROWS:
        row = pixels[y * width * 3 : (y + 1) * width * 3]
        for x in (*range(WIDE_MARGIN), *range(width - WIDE_MARGIN, width)):
            black += row[x * 3 : x * 3 + 3] == b"\0\0\0"
    return black


def sweep_widescreen(args: argparse.Namespace) -> int:
    """Enter every room with widescreen on and print what the game decided for it, and how much of the side strips
    stayed black. Rooms that fail to load are listed, not counted. Always returns 0: it's a report."""
    routes = args.out / "routes"
    routes.mkdir(parents=True, exist_ok=True)
    numbers = args.scenes or range(lom_scenes.SCENE_COUNT)
    counts = collections.Counter()
    rows = []
    saved = {key: os.environ.get(key) for key in (WIDESCREEN_VARIABLE, CAPTURE_VARIABLE)}
    os.environ[WIDESCREEN_VARIABLE] = os.environ[CAPTURE_VARIABLE] = "1"
    try:
        for number in numbers:
            try:
                route = sweep_route(number, lom_scenes.change_scenes_into(args.disc, number))
            except Exception as error:
                rows.append(f"{number:4d}  not entered: {error}")
                continue
            path = routes / f"{route['id']}.json"
            path.write_text(json.dumps(route, indent=1) + "\n")
            side_args = argparse.Namespace(**{**vars(args), "baselines": args.out / "baselines"})
            try:
                passed, report = run_case(Cases(side_args).case(str(path)), side_args, "record")
            except RouteError as error:
                rows.append(f"{number:4d}  not entered: {error}")
                continue
            folder = args.out / route["id"]
            decided = re.findall(r"FIELD widescreen: scene (\d+), map (-?\d+) px: ([a-z]+(?: [a-z]+)*)(?: \(layer (\d+) flags (\w+) scroll (\d+)\))?",
                                 (folder / "runner.log").read_text(errors="replace") if (folder / "runner.log").exists() else "")
            mine = [d for d in decided if int(d[0]) == number]
            picture = read_ppm(folder / "record" / "capture-0.ppm")
            if not passed or not mine:
                rows.append(f"{number:4d}  didn't settle in the room: {report.splitlines()[-1].strip()[:100]}")
                counts["not reached"] += 1
                continue
            _, width, status, layer, flags, scroll = mine[-1]
            status = status.strip()
            counts[status] += 1
            detail = f" (layer {layer} flags {flags} scroll {scroll})" if layer else ""
            if picture is None:
                rows.append(f"{number:4d}  {width:>5} px  {status}{detail}, left the field before the picture")
                print(rows[-1], flush=True)
                continue
            shown = re.search(r"^checkpoint \S+ frame \d+ scene (\d+)", (folder / "record" / "frames.txt").read_text(errors="replace"), re.M)
            if shown and int(shown.group(1)) != number:
                black = f", moved on to scene {shown.group(1)} by the picture"
            elif picture[0] == WIDE_WIDTH:
                black = f", {black_side_pixels(picture)} black pixels in the side strips"
            else:
                black = ", but the picture is 4:3"
            rows.append(f"{number:4d}  {width:>5} px  {status}{detail}{black}")
            print(rows[-1], flush=True)
    finally:
        for key, value in saved.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value
    summary = "\n".join(rows) + "\n\n" + "\n".join(f"{status}: {count}" for status, count in counts.most_common())
    (args.out / "sweep.txt").write_text(summary + "\n")
    print(f"\n{summary}\nthe table is in {args.out / 'sweep.txt'}, the pictures in each room's record folder")
    return 0


def record_route(path: Path, route: dict, args: argparse.Namespace) -> tuple[bool, str]:
    """Write a generated route and record it. When a talk's walk set off an exit on the way, record it again with
    its talks keeping clear of the exits (the clearances the exit generator uses)."""
    path.write_text(json.dumps(route, indent=2) + "\n")
    passed, report = run_route(path, args, True)
    for clearance, regions in [*((c, False) for c in CLEARANCES), (CLEARANCES[-1], True)]:
        if passed or WALKED_INTO_SCENE_CHANGE not in report or not any("talk_to" in step for step in route["steps"]):
            break
        route = {**route, "steps": [{**step, "talk_to": {**step["talk_to"], "clearance": clearance, **({"avoid_regions": True} if regions else {})}}
                                    if "talk_to" in step else step for step in route["steps"]]}
        path.write_text(json.dumps(route, indent=2) + "\n")
        earlier = report
        passed, report = run_route(path, args, True)
        report = f"{earlier}\n{report}"
    return passed, report

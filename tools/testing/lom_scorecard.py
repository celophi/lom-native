#!/usr/bin/env python3
from __future__ import annotations

import argparse
from dataclasses import dataclass
import functools
import glob
import json
import re
import struct
import sys
from pathlib import Path

DESCRIPTION = """Print the route tests' scorecard: what the route runs covered, against the whole game.

    lom_scorecard.py --disc <disc> --runs <folder> [--area HTW --area-name Domina] [--cache <file>]
                     [--explained tests/routes/explained.json] [--backlog <file>]

<folder> is where lom_routes.py put its runs (--out): each route's
record/coverage.txt is read and combined. The denominators are counted from
your disc with the decomp's scene tools; counting all 930 scenes takes a
minute or two, so they're cached in --cache.

Only routes that passed count (lom_routes.py leaves a "passed" marker in a
route's run folder), because a failed run or a failed generator attempt
proves nothing about what it reached.

Each row prints what's covered, what's explained (a written reason it isn't
covered: unused content, a do-nothing template, a story state no generator
reaches yet) and what's left. Reasons come from --explained, a JSON file of
row -> item -> reason, plus the ones the data proves on its own: layout
records whose event scripts are all a bare return. --backlog writes every
item that's neither, which is the work list.

Every recorded script instruction must match a decoded instruction in the scene
data. The share that does is printed first: if it ever drops below 100%, the
recorder or the decoder is wrong, and the numbers below it can't be trusted.
"""

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lom_scenes

CACHE_FORMAT = 2
SETUP_SLOTS = {14, 15}
MUSIC_FOLDERS = ("SOUND", "SND_SOTO")
SONG_LENGTH_LIMIT = 0x8000
WORLD_MAP_SCENE = 0x7FFF
GAME_CODE = "/game/src/"
PASSED_MARKER = "passed"


def scene_inventory(disc: Path, number: int, entered_objects: set[int]) -> dict:
    """The countable items of one scene."""
    lom_scenes._decomp_tools()
    from tools.data.scenes.event_scripts import describe_variable
    from tools.data.scenes.layout import read_layout_records
    from tools.data.scenes.scene_resources import read_resources

    scene = lom_scenes.scene(disc, number)
    instructions = scene.scripts["instructions"]
    shops, story, texts = set(), set(), 0
    for instruction in instructions:
        command, operands = instruction["command"], instruction["operands"]
        if command == "open_shop":
            shops.add(operands["list_index"].get("value"))
        if command == "open_text_window":
            texts += 1
        written = lom_scenes.written_variable(instruction)
        if written is not None and describe_variable(written)["scope"] == "saved_game":
            story.add(written)
    exits = set()
    for map_object in sorted(entered_objects or {0}):
        for marker in scene.markers(map_object):
            if marker.targets:
                exits.add(marker.label)
    monsters = set()
    for resource in read_resources(scene.data, scene.header)["resources"]:
        if resource.get("kind") == "battle":
            monsters |= {monster["id"] for monster in resource["battle"]["monsters"]}
    return {
        "instructions": len(instructions),
        "texts": texts,
        "shops": sorted(s for s in shops if s is not None),
        "story": sorted(story),
        "exits": sorted(exits),
        "monsters": sorted(monsters),
        "records": len(read_layout_records(scene.data, scene.header)),
    }


def song_ids(disc: Path) -> set[int]:
    """Every AKAO song sequence (non-zero length) in the music folders, found by scanning."""
    songs = set()
    for folder in MUSIC_FOLDERS:
        for path in glob.glob(str(disc / folder / "*")):
            data = Path(path).read_bytes()
            at = data.find(b"AKAO")
            while at != -1:
                if at + 16 <= len(data):
                    song, length = struct.unpack_from("<HH", data, at + 4)
                    if 0 < length < SONG_LENGTH_LIMIT:
                        songs.add(song)
                at = data.find(b"AKAO", at + 4)
    return songs


def inventory(disc: Path, cache: Path | None) -> dict:
    """The whole game's countable items, per scene, from the disc (cached)."""
    if cache and cache.exists():
        cached = json.loads(cache.read_text())
        if cached.get("format") == CACHE_FORMAT:
            return cached
    entered: dict[int, set[int]] = {}
    for number in range(lom_scenes.SCENE_COUNT):
        for instruction in lom_scenes.scene(disc, number).scripts["instructions"]:
            if instruction["command"] == "change_scene":
                operands = instruction["operands"]
                target = operands["scene_id"].get("value")
                if target is not None:
                    entered.setdefault(target & lom_scenes.SCENE_MASK, set()).add(operands["object_id"].get("value") or 0)
    scenes = {number: scene_inventory(disc, number, entered.get(number, set())) for number in range(lom_scenes.SCENE_COUNT)}
    result = {"format": CACHE_FORMAT, "scenes": {str(k): v for k, v in scenes.items()}, "songs": sorted(song_ids(disc))}
    if cache:
        cache.write_text(json.dumps(result))
    return result


def read_coverage(runs: Path) -> dict[str, set[tuple[int, ...]]]:
    """The coverage items of every route run that passed, combined."""
    items: dict[str, set[tuple[int, ...]]] = {}
    for path in runs.glob("*/record/coverage.txt"):
        if not (path.parent.parent / PASSED_MARKER).exists():
            continue
        for line in path.read_text().splitlines():
            kind, *values = line.split()
            items.setdefault(kind, set()).add(tuple(int(v) for v in values))
    return items


def executed_instructions(disc: Path, coverage: dict) -> tuple[dict[int, set[int]], int, int]:
    """Map recorded instructions to decoded ones. Returns scene -> offsets, recorded count, matched count."""
    executed: dict[int, set[int]] = {}
    recorded = matched = 0
    cache: dict[int, tuple] = {}

    def decoded(number: int):
        """A scene's event scripts' start, and its instructions by where they start and by where they end (with opcode)."""
        if number not in cache:
            scene = lom_scenes.scene(disc, number)
            by_start = {i["offset"]: i for i in scene.scripts["instructions"]}
            by_end = {(i["offset"] + i["size"], i["opcode"]): i for i in scene.scripts["instructions"]}
            cache[number] = (scene.header.event_scripts, by_start, by_end)
        return cache[number]

    for number, offset in coverage.get("instruction", set()):
        recorded += 1
        if number < lom_scenes.SCENE_COUNT:
            base, by_start, _ = decoded(number)
            if base + offset in by_start:
                executed.setdefault(number, set()).add(base + offset)
                matched += 1
    for number, end, opcode in coverage.get("instruction_end", set()):
        recorded += 1
        if number < lom_scenes.SCENE_COUNT:
            base, _, by_end = decoded(number)
            instruction = by_end.get((base + end, opcode))
            if instruction:
                executed.setdefault(number, set()).add(instruction["offset"])
                matched += 1
    return executed, recorded, matched


def covered_items(disc: Path, executed: dict[int, set[int]]) -> dict:
    """Shops, story variables, text windows and layout records reached through the executed instructions."""
    lom_scenes._decomp_tools()
    from tools.data.scenes.event_scripts import describe_variable
    from tools.data.scenes.layout import read_layout_records

    shops, story, texts, records, records_events = set(), set(), set(), set(), set()
    for number, offsets in executed.items():
        scene = lom_scenes.scene(disc, number)
        by_start = {i["offset"]: i for i in scene.scripts["instructions"]}
        for offset in offsets:
            instruction = by_start[offset]
            command, operands = instruction["command"], instruction["operands"]
            if command == "open_shop" and operands["list_index"].get("value") is not None:
                shops.add(operands["list_index"]["value"])
            if command == "open_text_window":
                texts.add((number, offset))
            written = lom_scenes.written_variable(instruction)
            if written is not None and describe_variable(written)["scope"] == "saved_game":
                story.add(written)
        entries = {index: entry.get("target_offset") for index, entry in enumerate(scene.scripts["entries"])}
        for index, record in enumerate(read_layout_records(scene.data, scene.header)):
            slots = [slot for slot, ref in enumerate(record.scripts)
                     if lom_scenes.event_script(ref) is not None and entries.get(lom_scenes.event_script(ref)) in offsets]
            if slots:
                records.add((number, index))
            if any(slot not in SETUP_SLOTS for slot in slots):
                records_events.add((number, index))
    return {"shops": shops, "story": story, "texts": texts, "records": records, "records_events": records_events}


def percent(part: int, whole: int) -> str:
    """A share as a percentage to one place, or "-" when there's nothing to share."""
    return f"{100 * part / whole:.1f}%" if whole else "-"


@functools.lru_cache(maxsize=None)
def macro_sites(header: str) -> dict[str, str]:
    """For a header whose macros generate functions (WMAP's step sequences): each generated function's name, to the
    .c file next to it that names it as a macro's first argument."""
    root = Path(header).parent.parent
    sites = {}
    for source in sorted(root.rglob("*.c")):
        for match in re.finditer(r"^\s*[A-Z][A-Z0-9_]*\(\s*(\w+)\s*[,)]", source.read_text(errors="replace"), re.M):
            sites.setdefault(match.group(1), str(source))
    return sites


def code_report(runs: Path, binary: Path, explained: dict, backlog: list[str]) -> dict | None:
    """The game code row, from the profiles a coverage-build run left (one runner.profraw per passing route):
    functions in the decomp's code that ran, out of all of them. None when the run has no profiles."""
    import subprocess

    profiles = [path for path in runs.glob("*/runner.profraw") if (path.parent / PASSED_MARKER).exists()]
    if not profiles:
        return None
    merged = runs / "merged.profdata"
    listing = runs / "profiles.txt"
    listing.write_text("".join(f"{path}\n" for path in profiles))
    subprocess.run(["llvm-profdata", "merge", "-sparse", "-f", str(listing), "-o", str(merged)], check=True)
    exported = subprocess.run(["llvm-cov", "export", str(binary), f"-instr-profile={merged}", "-skip-expansions", "-format=text",
                               "-summary-only=false"], check=True, capture_output=True, text=True).stdout
    functions = {}
    for function in json.loads(exported)["data"][0]["functions"]:
        name = function["filenames"][function["regions"][0][5]] if function.get("regions") else function["filenames"][0]
        if name.endswith(".h"):
            name = next((other for other in function["filenames"] if other.endswith(".c")), None) or macro_sites(name).get(function["name"], name)
        if GAME_CODE in name:
            key = f"game/src/{name.split(GAME_CODE, 1)[1]}:{function['name']}"
            functions[key] = functions.get(key, 0) + function["count"]
    covered = {key for key, count in functions.items() if count}
    reasons = {key for key in functions if key not in covered and key in explained}
    left = sorted(set(functions) - covered - reasons)
    backlog += [f"code {key}" for key in left]
    return {"covered": len(covered), "explained": len(reasons), "left": len(left), "of": len(functions)}


def placeholder_records(disc: Path, numbers: list[int]) -> set[tuple[int, int]]:
    """Layout records whose event scripts (slots 0 to 13) are all a bare return: nothing to trigger."""
    lom_scenes._decomp_tools()
    from tools.data.scenes.layout import read_layout_records

    out = set()
    for number in numbers:
        here = lom_scenes.scene(disc, number)
        for index, record in enumerate(read_layout_records(here.data, here.header)):
            events = [lom_scenes.event_script(ref) for slot, ref in enumerate(record.scripts)
                      if slot not in SETUP_SLOTS and lom_scenes.event_script(ref) is not None]
            if all([i["command"] for i in here.links.resolve(entry)["instructions"]] == ["return"] for entry in events):
                out.add((number, index))
    return out


@dataclass
class Evidence:
    game: dict
    scenes: dict[int, dict]
    coverage: dict
    executed: dict
    covered: dict
    written: dict
    placeholders: set

    def reasons(self, row: str) -> dict[tuple, str]:
        """A row's written reasons, by item (keys like "36:2" become (36, 2)); keys starting with _ are notes."""
        return {tuple(int(part, 0) for part in str(key).split(":")): reason
                for key, reason in self.written.get(row, {}).items() if not key.startswith("_")}


def scorecard_rows(evidence: Evidence, numbers: list[int]):
    """(key, label, all items, covered items, explained items) per row, then the text and instruction counts."""
    scenes, coverage, covered, reasons = evidence.scenes, evidence.coverage, evidence.covered, evidence.reasons
    chosen = set(numbers)
    exits_all = {(n, label) for n in chosen for label in scenes[n]["exits"]}
    records_all = {(n, i) for n in chosen for i in range(scenes[n]["records"])}
    placeholders = {r: "a do-nothing template: every event script is a bare return" for r in evidence.placeholders if r[0] in chosen}
    table = [
        ("scenes", "Scenes entered", {(n,) for n in chosen}, {s for s in coverage.get("scene", set()) if s[0] in chosen}, reasons("scenes")),
        ("exits", "Exits walked (walk-in markers)", exits_all,
         {(s, label) for s, label, _ in coverage.get("exit", set()) if (s, label) in exits_all}, reasons("exits")),
        ("shops", "Shops used (shop lists opened)", {(shop,) for n in chosen for shop in scenes[n]["shops"]},
         {(shop,) for shop in covered["shops"]}, reasons("shops")),
        ("songs", "Songs played (whole game)", {(song,) for song in evidence.game["songs"]}, set(coverage.get("song", set())), reasons("songs")),
        ("records", "Layout records whose scripts ran", records_all, {r for r in covered["records"] if r[0] in chosen}, reasons("records")),
        ("records_events", "  of them, an event ran (not just set-up)", records_all, {r for r in covered["records_events"] if r[0] in chosen},
         {**placeholders, **reasons("records_events")}),
        ("monsters", "Monsters fought (placed here)", {(m,) for n in chosen for m in scenes[n]["monsters"]},
         {(m,) for _, m in coverage.get("monster", set())}, reasons("monsters")),
        ("story", "Story variables written (written here)", {(ref,) for n in chosen for ref in scenes[n]["story"]},
         {(ref,) for ref in covered["story"]}, reasons("story")),
    ]
    texts = (len({t for t in covered["texts"] if t[0] in chosen}), sum(scenes[n]["texts"] for n in chosen))
    instructions = (sum(len(evidence.executed.get(n, ())) for n in chosen), sum(scenes[n]["instructions"] for n in chosen))
    return table, texts, instructions


def print_row(label: str, covered: int, explained: int, left: int, of: int) -> None:
    """Print one scorecard row: covered, explained and left of the whole, and how much is covered and done."""
    print(f"  {label:42s} {covered:8d} {explained:9d} {left:6d} {of:6d} {percent(covered, of):>8s} {percent(covered + explained, of):>7s}")


def print_count(label: str, done: int, of: int) -> None:
    """Print a scorecard row that's only a count, with nothing to explain."""
    print(f"  {label:42s} {done:8d} {'':>9s} {'':>6s} {of:6d} {percent(done, of):>8s}")


def report(disc: Path, runs: Path, cache: Path | None, explained_file: Path | None, backlog: Path | None,
           area: tuple[str, list[int]] | None = None, binary: Path | None = None) -> dict:
    """Print the scorecard. Returns each row's numbers for the whole game."""
    game = inventory(disc, cache)
    scenes = {int(k): v for k, v in game["scenes"].items()}
    coverage = read_coverage(runs)
    executed, recorded, matched = executed_instructions(disc, coverage)
    evidence = Evidence(game=game, scenes=scenes, coverage=coverage, executed=executed, covered=covered_items(disc, executed),
                        written=json.loads(explained_file.read_text()) if explained_file and explained_file.exists() else {},
                        placeholders=placeholder_records(disc, sorted(scenes)))
    print(f"script instructions recorded: {recorded}, matched to the disassembly: {matched} ({percent(matched, recorded)})")
    if recorded != matched:
        print("  the recorder and the disassembly disagree; check this before trusting the rows below")

    summary = {}
    reports = [("Whole game", sorted(scenes))] if area is None else [area, ("Whole game", sorted(scenes))]
    left_items = []
    for title, numbers in reports:
        table, texts, instructions = scorecard_rows(evidence, numbers)
        whole_game = title == "Whole game"
        print(f"\n{title} ({len(numbers)} scenes)")
        print(f"  {'Row':42s} {'covered':>8s} {'explained':>9s} {'left':>6s} {'of':>6s} {'covered':>8s} {'done':>7s}")
        for key, label, items, done, why in table:
            done = done & items
            explained = {item for item in items - done if item in why}
            left = items - done - explained
            print_row(label, len(done), len(explained), len(left), len(items))
            if whole_game:
                summary[key] = {"covered": len(done), "explained": len(explained), "left": len(left), "of": len(items)}
                left_items += [f"{key} {':'.join(str(part) for part in item)}" for item in sorted(left)]
        if whole_game and binary is not None:
            code = code_report(runs, binary, evidence.written.get("code", {}), left_items)
            if code is not None:
                summary["code"] = code
                print_row("Game code functions run", code["covered"], code["explained"], code["left"], code["of"])
        print_count("Text windows opened", *texts)
        print_count("Script instructions run", *instructions)
    if backlog:
        backlog.write_text("".join(f"{item}\n" for item in left_items))
        print(f"\n{len(left_items)} item(s) neither covered nor explained, listed in {backlog}")
    return summary


def main() -> int:
    """Read the command line and print the scorecard."""
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--disc", type=Path, required=True)
    parser.add_argument("--runs", type=Path, required=True, help="lom_routes.py's --out folder")
    parser.add_argument("--area", help="scene file prefix to report on its own, like HTW")
    parser.add_argument("--area-name", help="the area's name in the report")
    parser.add_argument("--cache", type=Path, help="where to keep the disc counts between runs")
    parser.add_argument("--explained", type=Path, help="written reasons for items that aren't covered (row -> item -> reason)")
    parser.add_argument("--backlog", type=Path, help="write the items neither covered nor explained here")
    args = parser.parse_args()
    area = None
    if args.area:
        import lom_routes

        area = (args.area_name or args.area, [n for n, _ in lom_routes.area_scenes(args.area)])
    report(args.disc, args.runs, args.cache, args.explained, args.backlog, area)
    return 0


if __name__ == "__main__":
    sys.exit(main())

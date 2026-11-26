from __future__ import annotations

import argparse
import base64
import dataclasses
import hashlib
import json
import os
import subprocess
import threading
import time
from collections.abc import Callable
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import lom_saves
from lom_headless import headless

from .settings import (BASELINE_FORMAT, BASELINE_SUFFIX, BOUNDARY_BASELINE, BUNDLED_MODS_ROOT, HERE, MODS_CARDS_HASH_DIGITS, MODS_CARDS_PREFIX, MODS_LIST, MODS_ROOT,
                       NO_MODS_FOLDER, SAVE_DUMP, SET_ENTRY_SEPARATOR)
from .plans import RouteError, load_route, make_plan, other_game_save, save_description
from .records import as_baseline, check_written_save, compare, describe_changes, read_record


PASSED_MARKER = "passed"
EXPLAINED_FILE = "explained.json"
HANDWRITTEN_FOLDER = "handwritten"
GENERATED_FOLDER = "generated"
SET_FORMAT = 1
SET_ABOUT = ("Baselines of generated routes. A run replays each baseline's plan from its save. "
             "Made with lom_routes.py freeze from a generator's output; see tools/testing/lom_routes.py.")
SET_DETAIL = ("pictures", "changes")
OTHER_GAME_SAVE_NAME = "BASLUS-00000TEST"
RUN_TIMEOUT_SECONDS = 600


@dataclasses.dataclass
class Case:
    id: str
    description: str
    save: dict
    where: str
    baseline: dict | None
    store: Callable[[dict], str]
    route: dict | None = None
    route_path: Path | None = None
    expect_save: dict | None = None
    plan: str | None = None
    card_other_game: bool = False
    mods: str | None = None


def file_case(route_path: Path, args: argparse.Namespace) -> Case:
    """The case for a route file, with the baseline next to it (or in --baselines)."""
    route = load_route(route_path)
    if getattr(args, "baselines", None):
        args.baselines.mkdir(parents=True, exist_ok=True)
        baseline_path = args.baselines / (route["id"] + BASELINE_SUFFIX)
    else:
        baseline_path = route_path.with_name(route_path.name.removesuffix(".json") + BASELINE_SUFFIX)

    def store(baseline: dict) -> str:
        """Write the route's baseline file."""
        baseline_path.write_text(json.dumps(baseline, indent=1) + "\n")
        return str(baseline_path)

    return Case(route["id"], route["description"], save_description(route_path, route), str(route_path),
                json.loads(baseline_path.read_text()) if baseline_path.exists() else None, store, route, route_path, route.get("expect_save"),
                card_other_game=bool(route.get("card_other_game")), mods=route.get("mods"))


def run_route(route_path: Path, args: argparse.Namespace, recording: bool) -> tuple[bool, str]:
    """Run one route file; compare it with its baseline, or record the baseline. Returns whether it passed, and its report."""
    return run_case(file_case(route_path, args), args, "record" if recording else "run")


def mods_cards_folder(mods: str | None) -> str | None:
    """The card folder the game keeps saves in for a set of test mods, worked out the way src/mods/mods.cpp does: mods
    with scripts change gameplay, and get "mods-" and the start of the SHA-256 of their sorted ids, one per line. A mod the
    set's folder doesn't have is one of the examples that come with lom-native, as in the game.

    A line with "=" in it, like "seed=", is a setting, not a mod, since an id can't hold "=".

    Returns None when no enabled mod in the set has scripts, so the saves stay in the card root."""
    if not mods:
        return None
    folder = MODS_ROOT / mods
    lines = [line.strip() for line in (folder / MODS_LIST).read_text().splitlines()]
    ids = [line for line in lines if line and not line.startswith("#") and "=" not in line]
    manifests = {mod_id: (folder / mod_id if (folder / mod_id).is_dir() else BUNDLED_MODS_ROOT / mod_id) / "mod.json" for mod_id in ids}
    scripted = sorted({mod_id for mod_id, manifest in manifests.items() if json.loads(manifest.read_text()).get("scripts")})
    if not scripted:
        return None
    digest = hashlib.sha256("".join(mod_id + "\n" for mod_id in scripted).encode()).hexdigest()
    return MODS_CARDS_PREFIX + digest[:MODS_CARDS_HASH_DIGITS]


def prepare_case_folder(case: Case, args: argparse.Namespace, folder: Path, layout: dict) -> bytes:
    """Empty a route's folder of the last run's record, saves and passed marker, and put its saved game on its
    memory card, as many copies as it asks for, with another game's save beside it when it wants one.

    Returns the save it starts from."""
    record_folder = folder / "record"
    record_folder.mkdir(parents=True, exist_ok=True)
    for old in record_folder.glob(SAVE_DUMP.format("*")):
        old.unlink()
    passed_marker = folder / PASSED_MARKER
    if passed_marker.exists():
        passed_marker.unlink()
    save = lom_saves.build(case.save, args.disc, layout)
    mods_cards = mods_cards_folder(case_mods(case, args))
    card = (folder / "cards" / mods_cards if mods_cards else folder / "cards") / lom_saves.CARD_SLOT_FOLDER
    card.mkdir(parents=True, exist_ok=True)
    for old in card.iterdir():
        old.unlink()
    for serial in range(1, (case.route or {}).get("card_copies", 1) + 1):
        (card / lom_saves.save_name(serial, 1, args.disc)).write_bytes(save)
    if case.card_other_game:
        (card / OTHER_GAME_SAVE_NAME).write_bytes(other_game_save())
    return save


def case_mods(case: Case, args: argparse.Namespace) -> str | None:
    """The mod set a route runs with: --mods when it's given, for trying a mod against routes (their baselines won't
    match then), otherwise the route's own."""
    return getattr(args, "mods", None) or case.mods


def case_plan(case: Case, args: argparse.Namespace, expected: dict | None) -> tuple[str, str | None]:
    """The plan to run: the baseline's when there is one, so a run replays what was recorded, otherwise one made
    fresh from the route file. Returns (plan, fresh plan), the fresh one None when it wasn't made."""
    if case.route is None:
        return case.plan, None
    fresh_plan = None if expected and getattr(args, "replay_only", False) else make_plan(case.route_path, case.route, args.disc)
    return (expected["plan"] if expected else fresh_plan), fresh_plan


def run_runner(args: argparse.Namespace, folder: Path, plan: Path, mods: str | None = None) -> tuple[int, bool]:
    """Run the route runner on a plan, headless, with the route's own cards, logging to runner.log.

    Mods come only from the route's own set under tests/mods, when it names one, and never from the mods folder of
    whoever runs the tests. Returns (exit code, whether it hung)."""
    env = os.environ.copy()
    env["LOM_CARD_ROOT"] = str(folder / "cards")
    env["LOM_MODS_DIR"] = str(MODS_ROOT / mods) if mods else str(folder / NO_MODS_FOLDER)
    env["LOM_BUNDLED_MODS_DIR"] = str(BUNDLED_MODS_ROOT)
    env.setdefault("LOM_ROUTE_FAILURE_CAPTURE", str(folder / "failure.ppm"))
    env.setdefault("LOM_TEST_CLOCK", "virtual")
    env["LOM_REMASTER_ROOT"] = os.environ.get("LOM_ROUTE_REMASTER", "")
    env.setdefault("LLVM_PROFILE_FILE", str(folder / "runner.profraw"))
    with open(folder / "runner.log", "w") as log:
        try:
            command = headless([str(args.runner), str(args.disc), str(plan), str(folder / "record")], env)
            return subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=RUN_TIMEOUT_SECONDS).returncode, False
        except subprocess.TimeoutExpired:
            return -1, True


def run_case(case: Case, args: argparse.Namespace, mode: str) -> tuple[bool, str]:
    """Run one route. Mode "run" compares it with its baseline, "record" writes a new one, and "upgrade" writes one
    in the current format only when the run matches the old one exactly. Returns whether it passed, and its report."""
    started = time.monotonic()
    folder = args.out / case.id
    record_folder = folder / "record"
    passed_marker = folder / PASSED_MARKER
    layout = lom_saves.read_layout(args.layout)
    save = prepare_case_folder(case, args, folder, layout)
    expected = None if mode == "record" else case.baseline
    plan_text, fresh_plan = case_plan(case, args, expected)
    plan = folder / "plan.txt"
    plan.write_text(plan_text)

    code, hung = run_runner(args, folder, plan, case_mods(case, args))
    run = read_record(record_folder)
    if "damaged" not in run:
        start = save[layout["saved_game_offset"] : layout["saved_game_offset"] + layout["saved_game_bytes"]]
        run["changes"] = describe_changes(run, record_folder, start, layout)
    seconds = time.monotonic() - started
    summary = f"{case.id}: {len(run['frames'])} frames, {seconds:.1f} s"
    failed_steps = [note for note in run["notes"] if note.startswith("failed")]
    if hung:
        failed_steps.append(f"failed: the game hung (no end after {RUN_TIMEOUT_SECONDS} s)")

    if "damaged" in run:
        return False, f"FAIL {summary}\n  the run's record is damaged (a write failed; is the disk full?): {run['damaged'][:80]}"

    if code != 0 or failed_steps:
        detail = compare(expected, run)[:1] if expected else []
        return False, (f"FAIL {summary}\n  the route didn't finish (runner exit {code}): {'; '.join(failed_steps) or 'see runner.log'}"
                       + "".join(f"\n  first difference from the baseline: {line}" for line in detail))

    if case.expect_save:
        problem = check_written_save(folder / "cards", save, layout, case.expect_save, lom_saves.save_name_prefix(args.disc))
        if problem:
            return False, f"FAIL {summary}\n  {problem}"

    passed_marker.write_text("")
    if mode == "record":
        return True, f"RECORDED {summary} -> {case.store(as_baseline(case.id, case.description, plan_text, run))}"
    if expected is None:
        return False, f"FAIL {summary}\n  no baseline; record one with `lom_routes.py record` and commit it"
    differences = compare(expected, run)
    if fresh_plan is not None and fresh_plan != plan_text:
        differences.append("the route's plan has changed since its baseline was recorded (the route file or the game data changed)")
    if differences:
        passed_marker.unlink()
        return False, f"FAIL {summary}\n  " + "\n  ".join(differences)
    if mode == "upgrade" and expected.get("format", 1) < BASELINE_FORMAT:
        return True, f"UPGRADED {summary} -> {case.store(as_baseline(case.id, case.description, plan_text, run))}"
    return True, f"PASS {summary}"


class RouteSet:

    def __init__(self, path: Path):
        """Read a generated set of routes.

        @param path The set's file.
        """
        self.path = path
        self.data = json.loads(path.read_text())
        if self.data.get("format") != SET_FORMAT:
            raise RouteError(f"{path}: not a generated set (format {SET_FORMAT})")
        self.entries = {entry["id"]: entry for entry in self.data["routes"]}
        self.lock = threading.Lock()
        self.changed = False

    def case(self, route_id: str) -> Case:
        """The case for one of the set's routes."""
        entry = self.entries.get(route_id)
        if entry is None:
            raise RouteError(f"{self.path}: no route {route_id}")
        where = f"{self.path}{SET_ENTRY_SEPARATOR}{route_id}"
        if isinstance(entry["save"], str):
            save = lom_saves.load_description(self.path.parent / entry["save"])
        else:
            save = lom_saves.find_files(json.loads(json.dumps(entry["save"])), self.path.parent, where)
        if "scene" not in save.get("scene", {}):
            raise RouteError(f"{where}: a route's save has to say which scene it's in")

        def store(baseline: dict) -> str:
            """Keep the new baseline for write(), without the detail a set doesn't keep."""
            with self.lock:
                entry["baseline"] = slim_baseline(baseline)
                self.changed = True
            return where

        return Case(route_id, entry["baseline"]["description"], save, where, entry["baseline"], store, expect_save=entry.get("expect_save"),
                    plan=entry["baseline"]["plan"], card_other_game=bool(entry.get("card_other_game")))

    def write(self) -> None:
        """Save the set if a baseline in it changed."""
        if self.changed:
            self.data["routes"] = [self.entries[route_id] for route_id in sorted(self.entries)]
            self.path.write_text(json.dumps(self.data, indent=1) + "\n")
            self.changed = False


def slim_baseline(baseline: dict) -> dict:
    """A baseline as a generated set keeps it: the plan and the fingerprints, without the per-frame pictures and the
    change lists, which `compare` makes again when a route needs looking at."""
    return {key: value for key, value in baseline.items() if key not in SET_DETAIL}


def is_set(path: Path) -> bool:
    """Whether a file is a generated set (generated/<area>/<generator>.json) rather than a route file."""
    return path.parent.parent.name == GENERATED_FOLDER


def expand_routes(paths: list[Path]) -> list[str]:
    """Route files as they are, and each generated set as one <set>#<id> for every route in it."""
    out = []
    for path in paths:
        if is_set(path) and SET_ENTRY_SEPARATOR not in path.name:
            out += [f"{path}{SET_ENTRY_SEPARATOR}{route_id}" for route_id in sorted(RouteSet(path).entries)]
        else:
            out.append(str(path))
    return out


class Cases:

    def __init__(self, args: argparse.Namespace):
        """Keep the generated sets the routes come from, to update their baselines.

        @param args The command line.
        """
        self.args = args
        self.sets: dict[Path, RouteSet] = {}
        self.lock = threading.Lock()

    def case(self, name: str) -> Case:
        """The case a command-line name stands for."""
        if SET_ENTRY_SEPARATOR not in name:
            return file_case(Path(name), self.args)
        path, route_id = name.rsplit(SET_ENTRY_SEPARATOR, 1)
        with self.lock:
            if Path(path) not in self.sets:
                self.sets[Path(path)] = RouteSet(Path(path))
            route_set = self.sets[Path(path)]
        case = route_set.case(route_id)
        if getattr(self.args, "baselines", None):
            self.args.baselines.mkdir(parents=True, exist_ok=True)
            baseline_path = self.args.baselines / (route_id + BASELINE_SUFFIX)
            case.baseline = json.loads(baseline_path.read_text()) if baseline_path.exists() else None

            def store(baseline: dict) -> str:
                """Write the reference baseline."""
                baseline_path.write_text(json.dumps(baseline, indent=1) + "\n")
                return str(baseline_path)

            case.store = store
        return case

    def write(self) -> None:
        """Save every set a record or upgrade changed."""
        for route_set in self.sets.values():
            route_set.write()


def baseline_of(name: str) -> dict:
    """The baseline of a route file or a <set>#<id>."""
    if SET_ENTRY_SEPARATOR in name:
        path, route_id = name.rsplit(SET_ENTRY_SEPARATOR, 1)
        return RouteSet(Path(path)).entries[route_id]["baseline"]
    path = Path(name)
    return json.loads(path.with_name(path.name.removesuffix(".json") + BASELINE_SUFFIX).read_text())


def route_id_of(name: str) -> str:
    """The id of a route file or a <set>#<id>."""
    return name.rsplit(SET_ENTRY_SEPARATOR, 1)[1] if SET_ENTRY_SEPARATOR in name else json.loads(Path(name).read_text())["id"]


def freeze(folders: list[Path], into: Path) -> int:
    """Keep the recorded routes in generator output folders as entries of a generated set, adding to it or replacing
    entries with the same id. A save in the routes' own folders goes into the entry; one elsewhere under tests/routes
    (a hand-written save) is kept as a path. A route with no baseline is left out and listed."""
    if not is_set(into):
        raise RouteError(f"{into}: a generated set goes under a {GENERATED_FOLDER}/ folder")
    data = json.loads(into.read_text()) if into.exists() else {"format": SET_FORMAT, "about": SET_ABOUT, "routes": []}
    entries = {entry["id"]: entry for entry in data["routes"]}
    left_out = 0
    for folder in folders:
        for route_path in suite_routes(folder, flat=True):
            route = load_route(route_path)
            baseline_path = route_path.with_name(route_path.name.removesuffix(".json") + BASELINE_SUFFIX)
            if not baseline_path.exists():
                print(f"LEFT OUT {route_path}: no baseline")
                left_out += 1
                continue
            save_path = (route_path.parent / route["start"]["save"]).resolve()
            if HANDWRITTEN_FOLDER in save_path.parts:
                save = os.path.relpath(save_path, into.parent.resolve())
            else:
                save = json.loads(save_path.read_text())
                if "saved_game" in save:
                    found = lom_saves.find_files(dict(save), save_path.parent, str(save_path))["_saved_game_path"]
                    save["saved_game_base64"] = base64.b64encode(Path(found).read_bytes()).decode()
                    del save["saved_game"]
            entry = {"id": route["id"], "save": save, "baseline": slim_baseline(json.loads(baseline_path.read_text()))}
            if route.get("expect_save"):
                entry["expect_save"] = route["expect_save"]
            if route.get("card_other_game"):
                entry["card_other_game"] = True
            entries[route["id"]] = entry
    if not entries:
        print(f"{into}: no routes with baselines, so no set; {left_out} left out")
        return 1
    data["routes"] = [entries[route_id] for route_id in sorted(entries)]
    into.parent.mkdir(parents=True, exist_ok=True)
    into.write_text(json.dumps(data, indent=1) + "\n")
    print(f"{into}: {len(entries)} route(s); {left_out} left out")
    return 0 if left_out == 0 else 1


def suite_routes(root: Path, flat: bool = False) -> list[Path]:
    """Every route file under a folder (or only in it, with flat): the .json files other than baselines, saves,
    the explanations, the boundary baselines and the generated sets."""
    return sorted(path for path in (root.glob("*.json") if flat else root.rglob("*.json"))
                  if not path.name.endswith(BASELINE_SUFFIX) and not {"saves", GENERATED_FOLDER} & set(path.relative_to(root).parts)
                  and path.name not in (EXPLAINED_FILE, BOUNDARY_BASELINE.name))


def suite_sets(root: Path) -> list[Path]:
    """Every generated set under a folder."""
    return sorted(path for path in root.rglob("*.json") if is_set(path))


def run_suite(args: argparse.Namespace) -> int:
    """Run every route and print the numbers: routes passed and failed (with the command that reruns
    each failure), unstable routes when repeated, the run time, then the scorecard."""
    import lom_scorecard

    if args.set:
        routes = expand_routes(args.set)
    else:
        routes = [str(path) for path in suite_routes(args.routes, args.flat)] + ([] if args.flat else expand_routes(suite_sets(args.routes)))
    cases = Cases(args)
    owners: dict[str, list[str]] = {}
    for route in routes:
        owners.setdefault(cases.case(route).id, []).append(route)
    clashes = {route_id: where for route_id, where in owners.items() if len(where) > 1}
    if clashes:
        for route_id, where in sorted(clashes.items()):
            print(f"route id {route_id} is used by {len(where)} routes: {', '.join(where)}")
        return 1
    started = time.monotonic()
    results: dict[str, list[bool]] = {route: [] for route in routes}
    reports: dict[str, str] = {}
    base_out = args.out
    for repeat in range(args.repeat):
        args.out = base_out if args.repeat == 1 else base_out / f"run-{repeat + 1}"

        def attempt(route: str) -> tuple[str, bool, str]:
            """Run one route, turning a route that can't run into a failure."""
            try:
                ok, report = run_case(cases.case(route), args, "run")
            except RouteError as error:
                ok, report = False, f"FAIL {route}\n  {error}"
            return route, ok, report

        with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
            for route, ok, report in pool.map(attempt, routes):
                results[route].append(ok)
                if not ok:
                    reports.setdefault(route, report)
        print(f"run {repeat + 1} of {args.repeat}: {sum(r[-1] for r in results.values())} of {len(routes)} passed", flush=True)
    seconds = time.monotonic() - started
    failed = [route for route, runs in results.items() if not any(runs)]
    unstable = [route for route, runs in results.items() if any(runs) and not all(runs)]
    common = f"--disc {args.disc} --runner {args.runner} --layout {args.layout} --out <folder>"
    for route in failed + unstable:
        print(reports[route])
        print(f"  rerun: python3 -I {HERE / 'lom_routes.py'} run '{route}' {common}")
    print(f"\nroutes: {len(routes)}, passed every time {len(routes) - len(failed) - len(unstable)}, failed {len(failed)}, "
          f"unstable {len(unstable)} (over {args.repeat} run(s)), {seconds / 60:.1f} minutes")
    args.out = base_out if args.repeat == 1 else base_out / "run-1"
    if args.no_scorecard:
        return 0 if not failed and not unstable else 1
    lom_scorecard.report(args.disc, args.out, args.cache, args.routes / EXPLAINED_FILE, args.backlog, binary=args.runner)
    return 0 if not failed and not unstable else 1

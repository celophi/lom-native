#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import random
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

DESCRIPTION = """Plant faults in the game and count how many the route tests catch.

Routes are only worth having if they fail when the game is wrong, and this
measures how often they do. Two kinds of fault:

    lom_faults.py data --disc <disc> --runs <folder> --routes <route.json>... --count N --seed S --work <folder>
    lom_faults.py code --lcov <file> --profiles <folder> --build <build dir> --routes <route.json>... --count N --seed S --work <folder>

Data faults change one operand byte of one script instruction on a copy of the
disc (a folder of links to your disc, with that one scene file replaced). The
sample comes from instructions the routes executed (from their coverage
records in --runs), so every fault is in something the routes ran; as a
control, as many again come from instructions in the same scenes the routes
never ran, which must not be caught. Each fault runs the routes that visited
that scene.

Code faults (mutants) change one operator or constant on one line of game code
the routes executed (from a clang source-coverage profile exported as lcov),
rebuild the runner, run the routes whose own profiles (in --profiles, one
<route>.lcov each) show they executed that line, and restore the file. The
build directory has to be a different one from any run in progress.

A fault is caught when any of its routes fails: a step fails, or the run
differs from its baseline in any channel.

A missed fault may change nothing the game does on those routes (a value
overwritten before it's read, a branch offset that lands on an equivalent
instruction): no test could catch it. So each missed fault's routes are
recorded again with and without it, writing PS1 RAM at every checkpoint
(LOM_ROUTE_RAM_DUMP), and a fault whose RAM matches (but for a data fault's
own byte, which is in the loaded scene) is reported as "no effect" and left
out of the score: caught out of the faults that changed the game.
"""

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import lom_routes
import lom_scenes
import lom_scorecard

ROUTES_COMMAND = HERE / "lom_routes.py"
ROUTES_PER_MUTANT = 6
BUILD_JOBS = 16
GAME_CODE = ("/generated/field_native/", "/generated/decomp_storage/src/")
RAM_COMPARE_BLOCK = 4096
CHECKPOINT_NOT_REACHED = 1 << 30
OPERATORS = [
    (re.compile(r"(?<![<>=!])<=(?!=)"), "<"), (re.compile(r"(?<![<>=!])>=(?!=)"), ">"),
    (re.compile(r"(?<![<>\-=])<(?![<=])"), "<="), (re.compile(r"(?<![<>\-=])>(?![>=])"), ">="),
    (re.compile(r"=="), "!="), (re.compile(r"!="), "=="),
    (re.compile(r"&&"), "||"), (re.compile(r"\|\|"), "&&"),
    (re.compile(r"(?<=\w|\)) \+ (?=[\w(])"), " - "), (re.compile(r"(?<=\w|\)) - (?=[\w(])"), " + "),
]
CONSTANT = re.compile(r"(?<![\w.])(0x[0-9A-Fa-f]+|\d+)(?![\w.])")


def route_ids(routes: list[Path]) -> dict[str, str]:
    """Route id to route file, or to <set>#<id> for a route in a generated set (a set given stands for all of them)."""
    return {lom_routes.route_id_of(name): name for name in lom_routes.expand_routes(routes)}


def run_routes(routes: list[Path], disc: Path, runner: Path, layout: Path, work: Path, args: argparse.Namespace) -> tuple[bool, str]:
    """Run routes against their baselines. Returns whether any failed, and the first failure's lines."""
    baselines = ["--baselines", str(args.baselines)] if args.baselines else []
    result = subprocess.run([sys.executable, "-I", str(ROUTES_COMMAND), "run", *map(str, routes), "--disc", str(disc), "--runner", str(runner),
                             "--layout", str(layout), "--out", str(work), "--replay-only", "--jobs", str(args.jobs), "--stop-at-failure", *baselines],
                            capture_output=True, text=True)
    lines = result.stdout.splitlines()
    failure = next((i for i, line in enumerate(lines) if line.startswith("FAIL")), None)
    return result.returncode != 0, " / ".join(lines[failure:failure + 2]) if failure is not None else ""


def ram_dumps(routes: list[Path], disc: Path, runner: Path, layout: Path, out: Path, args: argparse.Namespace) -> None:
    """Record routes with PS1 RAM written at every checkpoint (ram-<n>.bin in each route's record folder)."""
    shutil.rmtree(out, ignore_errors=True)
    subprocess.run([sys.executable, "-I", str(ROUTES_COMMAND), "record", *map(str, routes), "--disc", str(disc), "--runner", str(runner),
                    "--layout", str(layout), "--out", str(out), "--baselines", str(out / "baselines"), "--replay-only", "--jobs", str(args.jobs)],
                   env={**os.environ, "LOM_ROUTE_RAM_DUMP": "1"}, capture_output=True)


def ram_bytes_changed(base: Path, fault: Path, routes: list[Path]) -> int:
    """The most PS1 RAM bytes that differ at any checkpoint of any route between two ram_dumps runs (a checkpoint
    one run doesn't reach counts as a change)."""
    most = 0
    for route_id in route_ids(routes):
        for dump in sorted((base / route_id / "record").glob("ram-*.bin")):
            other = fault / route_id / "record" / dump.name
            if not other.exists():
                return CHECKPOINT_NOT_REACHED
            a, b = dump.read_bytes(), other.read_bytes()
            blocks = [(a[i:i + RAM_COMPARE_BLOCK], b[i:i + RAM_COMPARE_BLOCK]) for i in range(0, len(a), RAM_COMPARE_BLOCK)]
            most = max(most, sum(x != y for block_a, block_b in blocks if block_a != block_b for x, y in zip(block_a, block_b)))
    return most


def score(label: str, results: list[tuple[bool, bool]]) -> str:
    """The score line: caught out of the faults that changed the game, and the counts behind it."""
    caught = sum(1 for was_caught, _ in results if was_caught)
    no_effect = sum(1 for was_caught, effect in results if not was_caught and not effect)
    effective = len(results) - no_effect
    return (f"{label} caught: {caught} of {effective} that changed the game ({100 * caught / effective if effective else 0:.1f}%); "
            f"{no_effect} more changed nothing on their routes, {len(results)} planted")


def overlay(disc: Path, root: Path) -> Path:
    """A folder of links mirroring the disc, made once."""
    if not root.exists():
        for folder, _, files in os.walk(disc):
            target = root / Path(folder).relative_to(disc)
            target.mkdir(parents=True, exist_ok=True)
            for name in files:
                (target / name).symlink_to(Path(folder) / name)
    return root


def scenes_by_route(runs: Path, ids: dict[str, Path]) -> dict[int, list[Path]]:
    """Scene to the routes that entered it, from their coverage records."""
    out: dict[int, list[Path]] = {}
    for route_id, path in ids.items():
        coverage = runs / route_id / "record" / "coverage.txt"
        if coverage.exists():
            for line in coverage.read_text().splitlines():
                kind, *values = line.split()
                if kind == "scene":
                    out.setdefault(int(values[0]), []).append(path)
    return out


def executed_offsets(runs: Path, ids: dict[str, Path], disc: Path) -> dict[int, set[int]]:
    """Scene to the file offsets of the instructions the routes executed."""
    coverage: dict[str, set[tuple[int, ...]]] = {}
    for route_id in ids:
        path = runs / route_id / "record" / "coverage.txt"
        if path.exists():
            for line in path.read_text().splitlines():
                kind, *values = line.split()
                coverage.setdefault(kind, set()).add(tuple(int(v) for v in values))
    return lom_scorecard.executed_instructions(disc, coverage)[0]


def operand_bytes(instruction: dict) -> list[int]:
    """File offsets of an instruction's operand bytes (not its opcode or descriptor bytes)."""
    descriptors = len(instruction.get("descriptor_bytes", "").split())
    first = instruction["offset"] + 1 + descriptors
    return list(range(first, instruction["offset"] + instruction["size"]))


def data_faults(args: argparse.Namespace) -> int:
    """Plant data faults (and as many controls) one at a time, run each one's routes, and print the score."""
    ids = route_ids(args.routes)
    executed = executed_offsets(args.runs, ids, args.disc)
    visits = scenes_by_route(args.runs, ids)
    rng = random.Random(args.seed)
    covered, controls = [], []
    for number in sorted(executed):
        scene = lom_scenes.scene(args.disc, number)
        for instruction in scene.scripts["instructions"]:
            if operand_bytes(instruction):
                (covered if instruction["offset"] in executed[number] else controls).append((number, instruction))
    sample = [(True, item) for item in rng.sample(covered, min(args.count, len(covered)))]
    sample += [(False, item) for item in rng.sample(controls, min(args.count, len(controls)))]
    root = overlay(args.disc, args.work / "disc")
    results = []
    for is_covered, (number, instruction) in sample:
        path = lom_scenes.resource_paths()[lom_scenes.SCENE_RESOURCE_BASE + number]
        data = bytearray((args.disc / path).read_bytes())
        at = rng.choice(operand_bytes(instruction))
        data[at] ^= 1
        link = root / path
        link.unlink()
        link.write_bytes(bytes(data))
        started = time.time()
        caught, detail = run_routes(visits[number], root, args.runner, args.layout, args.work / "runs", args)
        effect = True
        if is_covered and not caught:
            ram_dumps(visits[number], root, args.runner, args.layout, args.work / "effect-fault", args)
        link.unlink()
        link.symlink_to(args.disc / path)
        if is_covered and not caught:
            ram_dumps(visits[number], args.disc, args.runner, args.layout, args.work / "effect-base", args)
            effect = ram_bytes_changed(args.work / "effect-base", args.work / "effect-fault", visits[number]) > 1
        name = f"scene {number} {instruction['command']} at {instruction['offset']:#x}, byte {at:#x}"
        results.append((is_covered, caught, effect))
        verdict = "CAUGHT" if caught else "missed " if effect else "no effect"
        print(f"{'covered' if is_covered else 'control'} {verdict} {name} ({len(visits[number])} routes, {time.time() - started:.0f} s) {detail}", flush=True)
    control_results = [caught for is_covered, caught, _ in results if not is_covered]
    print()
    print(score("data faults in executed instructions", [(caught, effect) for is_covered, caught, effect in results if is_covered]))
    print(f"controls (never executed) caught: {sum(control_results)} of {len(control_results)} (should be 0)")
    return 0


def baseline_frames(route: str) -> int:
    """How many frames a route's baseline has: format 1 lists them, formats 2 and 3 count them per stretch."""
    baseline = lom_routes.baseline_of(route)
    return len(baseline["frames"]) if "frames" in baseline else sum(stretch["frames"] for stretch in baseline["stretches"])


def read_lcov(path: Path) -> dict[str, set[int]]:
    """Game-code file (relative to the build's generated folder) to executed line numbers."""
    executed: dict[str, set[int]] = {}
    current = None
    for line in path.read_text().splitlines():
        if line.startswith("SF:"):
            current = line[3:]
        elif line.startswith("DA:") and current and any(part in current for part in GAME_CODE):
            number, hits = line[3:].split(",")[:2]
            if int(hits) > 0:
                executed.setdefault(current.split("/generated/", 1)[1], set()).add(int(number))
    return executed


def build_runner(build: Path) -> subprocess.CompletedProcess:
    """Rebuild the route runner in a build directory."""
    return subprocess.run(["cmake", "--build", str(build), "--target", "lom-route-runner", f"-j{BUILD_JOBS}"], capture_output=True, text=True)


def mutate(source: Path, number: int, start: int, end: int, replacement: str) -> str:
    """Put a mutant in a source file: replace part of one line. Returns the file as it was, to put back."""
    original = source.read_text()
    lines = original.split("\n")
    lines[number - 1] = lines[number - 1][:start] + replacement + lines[number - 1][end:]
    source.write_text("\n".join(lines))
    return original


def mutation_sites(path: Path, lines: set[int]) -> list[tuple[int, int, int, str]]:
    """(line, start, end, replacement) for every mutation on the given lines."""
    text = path.read_text().split("\n")
    sites = []
    for number in sorted(lines):
        line = text[number - 1]
        stripped = line.strip()
        if stripped.startswith(("#", "//", "/*", "*")) or '"' in line or "'" in line:
            continue
        for pattern, replacement in OPERATORS:
            sites += [(number, m.start(), m.end(), replacement) for m in pattern.finditer(line)]
        for m in CONSTANT.finditer(line):
            value = int(m.group(1), 0)
            sites.append((number, m.start(), m.end(), hex(value + 1) if m.group(1).startswith("0x") else str(value + 1)))
    return sites


def code_faults(args: argparse.Namespace) -> int:
    """Plant code mutants one at a time, rebuild and run each one's routes, check the missed ones changed anything,
    and print the score."""
    ids = route_ids(args.routes)
    executed = read_lcov(args.lcov)
    per_route = {route_id: read_lcov(args.profiles / f"{route_id}.lcov") for route_id in ids if (args.profiles / f"{route_id}.lcov").exists()}
    frames = {route_id: baseline_frames(path) for route_id, path in ids.items()}
    rng = random.Random(args.seed)
    sites = [(relative, *site) for relative, lines in sorted(executed.items()) if (args.build / "generated" / relative).exists()
             for site in mutation_sites(args.build / "generated" / relative, lines)]
    print(f"{len(executed)} files, {sum(map(len, executed.values()))} executed lines, {len(sites)} mutation sites", flush=True)
    results = []
    for relative, number, start, end, replacement in rng.sample(sites, min(args.count * 2, len(sites))):
        if len(results) >= args.count:
            break
        source = args.build / "generated" / relative
        original = mutate(source, number, start, end, replacement)
        before = original.split("\n")[number - 1]
        started = time.time()
        built = build_runner(args.build)
        label = f"{relative}:{number}: {before[start:end]} -> {replacement}"
        if built.returncode != 0:
            source.write_text(original)
            print(f"skip     {label} (doesn't compile)", flush=True)
            continue
        covering = sorted((route_id for route_id, lines_run in per_route.items() if number in lines_run.get(relative, ())), key=lambda r: frames[r])
        chosen = [ids[route_id] for route_id in covering[:ROUTES_PER_MUTANT]]
        if chosen:
            caught, detail = run_routes(chosen, args.disc, args.build / "lom-route-runner", args.build / "lom-save-layout", args.work / "runs", args)
        else:
            caught, detail = False, "no route ran it"
        source.write_text(original)
        results.append([caught, True, relative, number, start, end, replacement, chosen, label])
        print(f"{'CAUGHT' if caught else 'missed'} {label} ({len(chosen)} routes, {time.time() - started:.0f} s) {detail}", flush=True)
    build_runner(args.build)
    for result in results:
        caught, _, relative, number, start, end, replacement, chosen, label = result
        if caught or not chosen:
            continue
        runner, layout = args.build / "lom-route-runner", args.build / "lom-save-layout"
        ram_dumps(chosen, args.disc, runner, layout, args.work / "effect-base", args)
        source = args.build / "generated" / relative
        original = mutate(source, number, start, end, replacement)
        build_runner(args.build)
        ram_dumps(chosen, args.disc, runner, layout, args.work / "effect-fault", args)
        source.write_text(original)
        build_runner(args.build)
        result[1] = ram_bytes_changed(args.work / "effect-base", args.work / "effect-fault", chosen) > 0
        print(f"{'missed' if result[1] else 'no effect'} {label}", flush=True)
    print()
    print(score("mutants in executed game code", [(result[0], result[1]) for result in results]))
    return 0


def main() -> int:
    """Read the command line and plant the faults it asks for."""
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    data = commands.add_parser("data")
    data.add_argument("--runs", type=Path, required=True, help="lom_routes.py --out folder of a recent run, for coverage")
    data.add_argument("--runner", type=Path, required=True)
    data.add_argument("--layout", type=Path, required=True)
    code = commands.add_parser("code")
    code.add_argument("--lcov", type=Path, required=True, help="all routes' coverage, merged")
    code.add_argument("--profiles", type=Path, required=True, help="one <route id>.lcov per route")
    code.add_argument("--build", type=Path, required=True, help="the build to mutate; nothing else may be using it")
    for command in (data, code):
        command.add_argument("--disc", type=Path, required=True)
        command.add_argument("--routes", type=Path, nargs="+", required=True)
        command.add_argument("--count", type=int, default=30)
        command.add_argument("--seed", type=int, default=1)
        command.add_argument("--work", type=Path, required=True)
        command.add_argument("--jobs", type=int, default=1, help="how many of a fault's routes to run at once")
        command.add_argument("--baselines", type=Path, help="compare with the baselines in this folder (see lom_routes.py --baselines)")
    args = parser.parse_args()
    return data_faults(args) if args.command == "data" else code_faults(args)


if __name__ == "__main__":
    sys.exit(main())

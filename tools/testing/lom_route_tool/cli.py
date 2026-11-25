from __future__ import annotations

import argparse
import json
import threading
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from .settings import COMPACT_FORMAT, ROUTES_ROOT, SET_ENTRY_SEPARATOR
from .plans import RouteError, load_route
from .records import as_compact
from .cases import Cases, expand_routes, freeze, run_case, run_suite
from .comparisons import compare_boundaries, compare_builds, compare_remaster, compare_widescreen, sweep_widescreen
from .generate_scenes import generate_battles, generate_bosses, generate_exits, generate_scenes, generate_talks
from .generate_story import generate_story
from .generate_world import generate_lands, generate_places, generate_wmap_buttons, generate_wmap_events
from .generate_menus import generate_audio, generate_menus, generate_music, generate_record_events, generate_shopping
from .generate_chain import generate_chain


DESCRIPTION = """Run route tests against the game, and compare them with their baselines.

A route is a JSON file (see route.schema.json): start from this save, do these
steps, check these points. This command checks the route, builds its save,
turns its steps into a plan for lom-route-runner (looking up exits, people and
scene changes in the game data), runs it, and compares the run with the
route's baseline, <route>.baseline.json next to it (for a generated route,
its entry in a generated set; see below).

Running routes:

    lom_routes.py check <route.json>...
    lom_routes.py run <route.json>... --disc <disc> --runner <lom-route-runner> --layout <lom-save-layout> --out <folder>
    lom_routes.py record <route.json>...    (the same options; both take --jobs N to run N routes at once)
    lom_routes.py suite --disc <disc> --runner <lom-route-runner> --layout <lom-save-layout> --out <folder> [--repeat N]
        Run every route under tests/routes, print what failed and how to rerun it, then the scorecard.
    lom_routes.py upgrade <route>...    (the same options as run)
        Record baselines in the current format, keeping a new one only where the run matches the old one exactly.
    lom_routes.py convert-baselines <route.baseline.json>...
        Rewrite format 1 baselines in format 2, without running anything.

Generated routes aren't kept as route files. A generator decides what goes in
a route by running the game, so making them again on a changed build would let
that build choose. What pins them is the plan each baseline recorded.
One generator's routes for an area are one set file,
tests/routes/generated/<area>/<generator>.json, holding each route's id, its
save and its baseline. `run`, `record` and `upgrade` take a set (every route in
it) or <set>#<route id>; a recorded set entry keeps its plan. `suite` runs the
route files and every set.

    lom_routes.py freeze <generator output folder>... --into tests/routes/generated/<area>/<generator>.json
        Add a generator's recorded routes to a set, without running anything.

A set keeps only what catches a difference: the plan, the save, and the
fingerprints of each checkpoint and of each stretch of frames between them. To
see what a difference is, run the route on both builds:

    lom_routes.py compare <route> --runner <build A's lom-route-runner> --against <build B's> --disc <disc> --layout <lom-save-layout> --out <folder>
        Print what differs: the end and notes, each checkpoint's change list, the first frame that differs and in
        which channels, and for a picture the first GPU call that differs. --disc-b gives build B another disc.

Making routes from the game data. They all take
--disc <disc> --area <file prefix> --name <area name> --save <save, relative to --out> --out <folder>,
and with --validate (which every one but generate-exits needs) they record each
route as they write it and try other ways until one works. --validate also
needs --runner, --layout and --work, and generate-exits always needs --layout:

    generate-exits     one route per walk-in exit
    generate-talks     one route per placed person with an event, talking to them
    generate-battles   one route per battle zone, fighting what's there
    generate-scenes    a direct-entry route for each scene nothing enters with change_scene, and an
                       entry route for each scene with no walk-in exits (so no exit route enters it)
    generate-bosses    one route per scene whose battle starts from an event (a boss room): enter,
                       confirm through to the battle, fight, watch what follows
    generate-story     for each saved-game variable a scene's scripts write, a route that triggers the
                       write (entering the scene, or talking to someone) from a save its tests allow
    generate-shops     the same, for each shop list the scripts open
    generate-commands  the same, for each misc_command subcommand the scripts use (an item count, spending money)
    generate-lands     one route per land's music: leave for the world map and enter a land that plays it
    generate-places    one route per artifact: leave for the world map, put it on the map and enter its land
    generate-wmap-events  a route per world map event, from the save with only that event pending: onto the map,
                       then home (or watch it, where it never hands back control)
    generate-menus     a route per ring menu item and press pattern (MENU_PATTERNS): open the menu with Start, turn the
                       ring, press, back out
    generate-shopping  random purchases at a shopkeeper (--only <scene>:<record>, --random N), from the save with plenty of
                       lucre, so the game makes the items
    generate-workshop  random workshop visits (--only <scene>:<record>, --random N): the ring menu the talk opens, and
                       its screens
    generate-wmap-buttons  a route per world map button scenario (WMAP_BUTTON_SCENARIOS): the spirits' view, the map game
    generate-record-events  a route per event of the layout records --only names (<scene>:<record>), started on the
                       record's actor as talking does
    generate-music     a route per music id a scene change can carry, listening to its song for minutes
    generate-audio     a route per audio scenario (AUDIO_SCENARIOS): the game's own audio calls, while a song plays
    generate-chain     the story played forward from the save, a story write at a time: a route per step, each from
                       the saved game the step before left (--area ALL for every area; --jobs, --steps)

`run` passes when every step finishes and the run matches its baseline
exactly. `record` writes the baseline instead; a baseline only counts once it's
committed, which is how a change to one gets approved. A generator writes route
files into --out and records them there; `freeze` then keeps them in their set.
"""
CHAIN_STEPS = 500
GENERATOR_HELP = {
    "exits": "write one route per walk-in exit of an area",
    "talks": "write one route per placed actor with an event of an area, talking to it",
    "battles": "write one route per battle zone of an area, fighting what's there",
    "scenes": "write a direct-entry route for each scene of an area that no change_scene enters",
    "story": "write a route per saved-game variable an area's scripts write, triggering the write",
    "bosses": "write one route per scene of an area whose battle starts from an event (a boss room)",
    "shops": "write a route per shop list an area's scripts open, from a save its tests allow",
    "commands": "write a route per misc_command subcommand an area's scripts use, from a save its tests allow",
    "lands": "write a route per land music: leave the area for the world map and enter a land that plays it",
    "places": "write a route per artifact: leave the area for the world map, put the artifact on the map and enter its land",
    "wmap-events": "write a route per world map event, from the save with only that event pending",
    "menus": "write a route per ring menu item and press pattern, from the save in the field",
    "shopping": "write random shopping routes for the shopkeepers --only names (<scene>:<record>), --random of them each",
    "workshop": "write random workshop routes for the people --only names (<scene>:<record>), --random of them each: the ring menu their talk opens, and its screens",
    "wmap-buttons": "write a route per world map button scenario (WMAP_BUTTON_SCENARIOS): the spirits' view, the map game",
    "record-events": "write a route per event of the layout records --only names (<scene>:<record>), started on the record's actor as talking does",
    "music": "write a route per music id a scene change can carry, listening to its song for minutes, from the save",
    "audio": "write a route per audio scenario (AUDIO_SCENARIOS): the game's own audio calls, from the save where a song plays",
    "chain": "play the story forward from the save, a story write at a time, writing a route per step (--area ALL for every area)",
}


def build_parser() -> argparse.ArgumentParser:
    """The command line: check, run, record, suite, convert-baselines and the generators."""
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("check", help="check route files without running them").add_argument("routes", nargs="+", type=Path)
    for name, help_text in (("run", "run routes and compare them with their baselines"), ("record", "run routes and write their baselines"),
                            ("upgrade", "run routes and write their baselines in the current format, only where the run matches the old one")):
        command = commands.add_parser(name, help=help_text)
        command.add_argument("routes", nargs="+", type=Path, help="route files, generated sets, or <set>#<route id>")
        command.add_argument("--disc", type=Path, required=True)
        command.add_argument("--runner", type=Path, required=True)
        command.add_argument("--layout", type=Path, required=True)
        command.add_argument("--out", type=Path, required=True)
        command.add_argument("--replay-only", action="store_true",
                             help="don't check the plan against the game data (for planted-fault runs, where the data is deliberately changed)")
        command.add_argument("--jobs", type=int, default=1, help="how many routes to run at once (each is its own game process)")
        command.add_argument("--mods", help="run every route with this mod set from tests/mods instead of its own; baselines won't match")
        command.add_argument("--stop-at-failure", action="store_true", help="don't start any more routes once one fails (routes already running finish)")
        command.add_argument("--baselines", type=Path,
                             help="read and write baselines in this folder, named by route id, instead of next to the routes "
                                  "(for reference runs that aren't committed)")
    suite = commands.add_parser("suite", help="run every route under tests/routes and print the scorecard")
    suite.add_argument("--disc", type=Path, required=True)
    suite.add_argument("--runner", type=Path, required=True)
    suite.add_argument("--layout", type=Path, required=True)
    suite.add_argument("--out", type=Path, required=True)
    suite.add_argument("--routes", type=Path, default=ROUTES_ROOT, help="where the routes are (default tests/routes)")
    suite.add_argument("--set", type=Path, nargs="+", help="only the routes in these generated sets")
    suite.add_argument("--jobs", type=int, default=6, help="how many routes to run at once")
    suite.add_argument("--mods", help="run every route with this mod set from tests/mods instead of its own; baselines won't match")
    suite.add_argument("--repeat", type=int, default=1, help="run everything this many times; a route that doesn't pass every time is unstable")
    suite.add_argument("--cache", type=Path, help="where the scorecard keeps the disc counts between runs")
    suite.add_argument("--backlog", type=Path, help="write the scorecard items neither covered nor explained here")
    suite.add_argument("--flat", action="store_true", help="only the route files in --routes itself: not its subfolders or generated sets")
    suite.add_argument("--no-scorecard", action="store_true", help="just run the routes (ctest runs one folder at a time this way)")
    commands.add_parser("convert-baselines", help="rewrite format 1 baselines in format 2 (no runs)").add_argument("baselines", nargs="+", type=Path)
    compare_command = commands.add_parser("compare", help="run one route on two builds and print what differs")
    compare_command.add_argument("route", type=Path, help="a route file or <set>#<route id>")
    compare_command.add_argument("--runner", type=Path, required=True, help="build A's lom-route-runner")
    compare_command.add_argument("--against", type=Path, required=True, help="build B's lom-route-runner")
    compare_command.add_argument("--disc", type=Path, required=True)
    compare_command.add_argument("--disc-b", type=Path, help="another disc for build B (a planted data fault, say)")
    compare_command.add_argument("--layout", type=Path, required=True)
    compare_command.add_argument("--out", type=Path, required=True)
    widescreen = commands.add_parser("widescreen", help="run routes with widescreen off and on, and check only the picture changes, rightly")
    widescreen.add_argument("routes", nargs="+", type=Path, help="route files, folders of them, generated sets, or <set>#<route id>")
    widescreen.add_argument("--disc", type=Path, required=True)
    widescreen.add_argument("--runner", type=Path, required=True)
    widescreen.add_argument("--layout", type=Path, required=True)
    widescreen.add_argument("--out", type=Path, required=True)
    remaster = commands.add_parser("remaster", help="run routes without and with the Remaster, and check only the picture and sound change")
    remaster.add_argument("routes", nargs="+", type=Path, help="route files, folders of them, generated sets, or <set>#<route id>")
    remaster.add_argument("--disc", type=Path, required=True)
    remaster.add_argument("--runner", type=Path, required=True)
    remaster.add_argument("--layout", type=Path, required=True)
    remaster.add_argument("--out", type=Path, required=True)
    remaster.add_argument("--remaster", type=Path, required=True, help="the Remaster's folder (Steam's Legend of Mana)")
    remaster.add_argument("--music", type=int, choices=(0, 1, 2), default=1, help="0 the PS1's, 1 the original recordings, 2 the arranged ones")
    boundaries = commands.add_parser("boundaries", help="run routes and compare what crossed the platform boundaries with the stored baseline")
    boundaries.add_argument("routes", nargs="+", type=Path, help="route files, folders of them (searched all the way down, but not saves/), generated sets, or <set>#<route id>")
    boundaries.add_argument("--record", action="store_true", help="record the baseline for these routes instead of comparing")
    boundaries.add_argument("--disc", type=Path, required=True)
    boundaries.add_argument("--runner", type=Path, required=True)
    boundaries.add_argument("--layout", type=Path, required=True)
    boundaries.add_argument("--out", type=Path, required=True)
    sweep = commands.add_parser("widescreen-sweep", help="enter every room with widescreen on and report what the game decided and what's missing")
    sweep.add_argument("--disc", type=Path, required=True)
    sweep.add_argument("--runner", type=Path, required=True)
    sweep.add_argument("--layout", type=Path, required=True)
    sweep.add_argument("--out", type=Path, required=True)
    sweep.add_argument("--scenes", type=int, nargs="+", help="only these scene numbers")
    freeze_command = commands.add_parser("freeze", help="keep a generator's recorded routes as a generated set (no runs)")
    freeze_command.add_argument("folders", nargs="+", type=Path, help="generator output folders (--out of generate-<kind>)")
    freeze_command.add_argument("--into", type=Path, required=True, help="the set: tests/routes/generated/<area>/<generator>.json")
    for kind, help_text in GENERATOR_HELP.items():
        add_generate_arguments(commands.add_parser(f"generate-{kind}", help=help_text))
    return parser


def convert_baselines(paths: list[Path]) -> int:
    """Rewrite format 1 baselines in format 2, without running anything."""
    for path in paths:
        old = json.loads(path.read_text())
        if old.get("format", 1) < COMPACT_FORMAT:
            path.write_text(json.dumps(as_compact(old), indent=1) + "\n")
            print(f"CONVERTED {path}")
    return 0


def run_generator(parser: argparse.ArgumentParser, args: argparse.Namespace) -> int:
    """Write an area's routes with one of the generators, and say how many it couldn't make."""
    if args.validate and not (args.runner and args.layout and args.work):
        parser.error("--validate needs --runner, --layout and --work")
    if args.command == "generate-exits" and not args.layout:
        parser.error("generate-exits needs --layout: it reads the routes' save to work out story states")
    args.routes_out, args.out = args.out, args.work
    if args.command == "generate-exits":
        failed = generate_exits(args)
        print(f"done; {failed} exit(s) not generated")
        return 0 if failed == 0 else 1
    if not args.validate:
        parser.error(f"{args.command} needs --validate: its routes only count once the run shows what they test")
    generate = {"generate-talks": generate_talks, "generate-battles": generate_battles, "generate-scenes": generate_scenes,
                "generate-story": generate_story, "generate-bosses": generate_bosses, "generate-shops": generate_story,
                "generate-commands": generate_story,
                "generate-lands": generate_lands, "generate-places": generate_places, "generate-chain": generate_chain,
                "generate-wmap-events": generate_wmap_events, "generate-menus": generate_menus,
                "generate-shopping": generate_shopping, "generate-workshop": generate_shopping, "generate-audio": generate_audio,
                "generate-music": generate_music, "generate-record-events": generate_record_events,
                "generate-wmap-buttons": generate_wmap_buttons}[args.command]
    failed = generate(args)
    print(f"done; {failed} route(s) not generated")
    return 0 if failed == 0 else 1


def main() -> int:
    """Read the command line and run the command."""
    parser = build_parser()
    args = parser.parse_args()
    if args.command == "suite":
        return run_suite(args)
    if args.command == "convert-baselines":
        return convert_baselines(args.baselines)
    if args.command == "freeze":
        return freeze(args.folders, args.into)
    if args.command == "compare":
        return compare_builds(args)
    if args.command == "widescreen":
        return compare_widescreen(args)
    if args.command == "remaster":
        return compare_remaster(args)
    if args.command == "widescreen-sweep":
        return sweep_widescreen(args)
    if args.command == "boundaries":
        return compare_boundaries(args)
    if args.command.startswith("generate-"):
        return run_generator(parser, args)
    return run_routes(args)


def add_generate_arguments(generate: argparse.ArgumentParser) -> None:
    """The options every generator takes."""
    generate.add_argument("--disc", type=Path, required=True)
    generate.add_argument("--area", required=True, help="scene file prefix, like HTW")
    generate.add_argument("--name", required=True, help="the area's name in route ids, like domina")
    generate.add_argument("--save", required=True, help="the routes' save description, relative to --out")
    generate.add_argument("--out", type=Path, required=True, help="where the route files go")
    generate.add_argument("--validate", action="store_true", help="record each route as it's written, trying other ways in until one works")
    generate.add_argument("--runner", type=Path, help="with --validate")
    generate.add_argument("--layout", type=Path, help="the lom-save-layout program: with --validate, and always for generate-exits")
    generate.add_argument("--work", type=Path, help="with --validate: where runs go")
    generate.add_argument("--only", nargs="+", help="only these, by route file name without .json (like htw_sc00-exit-2 or htw_inn0-talk-1)")
    generate.add_argument("--jobs", type=int, default=1, help="generate-chain: how many candidates to run at once")
    generate.add_argument("--steps", type=int, default=CHAIN_STEPS, help="generate-chain: the most steps to take")
    generate.add_argument("--random", type=int, default=0, help="generate-menus: also this many random press patterns, seeds 1 to N")
    generate.add_argument("--presses", type=int, help="generate-shopping, generate-workshop and generate-record-events: random presses per route, instead of the kind's default")
    generate.add_argument("--skip", type=int, default=0, help="generate-shopping: move the cursor this many rows down the list before the random presses")
    generate.add_argument("--fight-for", type=int, default=0,
                          help="generate-bosses: fight this many frames, then go on whether the battle has ended or not")
    generate.add_argument("--rotate-targets", action="store_true",
                          help="generate-bosses: the fight goes after the next monster when its target's HP stops dropping")
    generate.add_argument("--trail", help="generate-shopping and generate-workshop: press these buttons after the others, before the checkpoint "
                                          "(circle,circle,start leaves the golem editor, which FIELD waits on)")
    generate.add_argument("--card-other-game", action="store_true",
                          help="generate-shopping and generate-workshop: put another game's save on the card too (its screens list it)")
    generate.add_argument("--cycle", help="generate-shopping and generate-workshop: repeat these buttons (like cross,down) instead of random presses")
    generate.add_argument("--lead", help="generate-shopping and generate-workshop: press these buttons (like right,right,cross) before the others; generate-battles: once the battle starts")


def run_routes(args: argparse.Namespace) -> int:
    """Check, run, record or upgrade the routes given."""
    stop = threading.Event()
    cases = Cases(args)

    def attempt(route: str) -> tuple[bool, str] | None:
        """Check or run one route; None when it wasn't started because an earlier one failed."""
        if stop.is_set():
            return None
        try:
            if args.command == "check":
                if SET_ENTRY_SEPARATOR in route:
                    cases.case(route)
                else:
                    load_route(Path(route))
                return True, f"OK {route}"
            ok, report = run_case(cases.case(route), args, args.command)
        except RouteError as error:
            ok, report = False, f"FAIL {route}\n  {error}"
        if not ok and getattr(args, "stop_at_failure", False):
            stop.set()
        return ok, report

    passed, not_run = True, 0
    with ThreadPoolExecutor(max_workers=max(1, getattr(args, "jobs", 1))) as pool:
        for result in pool.map(attempt, expand_routes(args.routes)):
            if result is None:
                not_run += 1
                continue
            print(result[1], flush=True)
            passed &= result[0]
    cases.write()
    if not_run:
        print(f"stopped at the first failure; {not_run} route(s) not run")
    return 0 if passed else 1

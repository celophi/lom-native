#!/usr/bin/env python3
from __future__ import annotations

import argparse
import collections
import json
import re
import sys
from pathlib import Path

DESCRIPTION = """Checks how the game reaches the platform.

The game should reach the platform only through the port's boundaries in
include/. This lists the two ways it still goes around them, side doors and
renames, and compares both with an allow list,
tools/testing/separation_allow.json, that shrinks as each area of the
separation work is done. It also checks the port doesn't take over the game's
own behavior:

- **side doors**: calls in the game files the native build compiles and the
  game's own headers (not
  psyq/, the decomp's copy of Sony's libraries, and not the files of the
  original backends below) to PlayStation
  library and kernel functions and macros, the ones
  declared in the platform SDK headers (the game's own in game/include/sdk and
  PSY-Z's). The 3D math (libgte) and the small C library headers are allowed,
  since they're math and C, not platform.
- **renames**: entries in tools/build/unit_globals.json's defines_by_file that
  rename one of those platform names at build time, which hides from the game
  code that the call goes somewhere else.

- **game behavior in src/**: port code that changes the game's state (which
  screen it's on, where FIELD starts, the saved game), runs the game's own
  flow (screen transitions, scene resets, songs between screens, the overlays'
  main functions) or drives the sound driver outside the sound backend. The
  game's rules live in game/; the port starts the hardware, loads, checks and
  enters overlays, and draws. This one has no allow list: the places allowed
  are below (PORT_ALLOWED).

The allow list has to match exactly: a new call or rename fails, and so does
one that's gone but still listed, so the list always says where things stand.
After removing some, run with --update to rewrite it, and check the diff.

    python3 tools/testing/separation_check.py           check
    python3 tools/testing/separation_check.py --update  rewrite the allow list
    python3 tools/testing/separation_check.py --report  totals by library and unit
"""

ROOT = Path(__file__).resolve().parents[2]
ALLOW = ROOT / "tools/testing/separation_allow.json"
UNIT_SETTINGS = ROOT / "tools/build/unit_globals.json"

PLATFORM_HEADERS = ["kernel.h", "libapi.h", "libcd.h", "libetc.h", "libgpu.h", "libmcx.h", "libpad.h", "libpress.h", "libspu.h"]
HEADER_FOLDERS = [ROOT / "game/include/sdk", ROOT / "external/psyz/psyz/include"]

ALLOWED_NAMES = {"getScratchAddr"}

BACKEND_FILES = ["main/audio/akao_*.c", "main/controller.c"]

GAME_STATE = ["g_game_state", "g_pending_game_state", "g_previous_game_state", "g_current_land", "g_field_object_id", "g_field_spawn_id",
              "g_field_entry_scene", "g_field_music_id", "g_field_sound_bank_id", "g_field_secondary_music_id", "g_saved_game"]
GAME_FLOW = ["screen_transition", "field_scene_reset", "field_restore_entry_music", "load_and_play_song", "cload_main", "run_field_scene",
             "run_world_map", "run_title", "gname_run", "run_checkps", "field_set_scene_parameters", "field_update_scene",
             "field_build_frame_commands", "field_draw_frame", "field_run_frame_loop", "main_game_loop", "movie_play"]
PORT_ALLOWED = {
    "src/audio/*": {"akao_*"},
    "src/field/field_music_stream_native.c": {"akao_*"},
    "src/overlays/title_entry.c": {"run_title"},
    "src/overlays/gname_entry.c": {"gname_run"},
}

DECLARATION = re.compile(r"\b([A-Za-z_]\w*)\s*\([^;{}()]*(?:\([^()]*\)[^;{}()]*)*\)\s*;")
MACRO = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)\(", re.MULTILINE)
TYPE_WORDS = {
    "void", "int", "long", "short", "char", "unsigned", "signed", "float", "double", "extern", "static", "const", "volatile",
    "u8", "u16", "u32", "u64", "s8", "s16", "s32", "s64", "u_long", "u_short", "u_char", "u_int", "size_t", "uintptr_t", "struct", "inline",
}


def strip_comments_and_strings(text: str) -> str:
    """C source with comments, string and character literals blanked, keeping line numbers."""
    def blank(match: re.Match) -> str:
        """Blank out a match, keeping its line breaks.

        @param match The match.
        @return Spaces and line breaks.
        """
        return re.sub(r"[^\n]", " ", match.group(0))

    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', blank, text, flags=re.DOTALL)


def platform_names() -> dict[str, str]:
    """Every platform function and function-like macro, with the header it's from."""
    names: dict[str, str] = {}

    for folder in HEADER_FOLDERS:
        for header in PLATFORM_HEADERS:
            path = folder / header

            if not path.exists():
                continue

            text = strip_comments_and_strings(path.read_text(errors="replace"))

            for match in MACRO.finditer(text):
                if match.group(1) not in ALLOWED_NAMES:
                    names.setdefault(match.group(1), header)

            body = "\n".join(line for line in text.splitlines() if not line.lstrip().startswith("#"))

            for match in DECLARATION.finditer(body):
                name = match.group(1)
                if name in ALLOWED_NAMES:
                    continue
                before = body[: match.start(1)].rstrip()

                if before and (before[-1] == "*" or re.search(r"[A-Za-z_]\w*$", before)) and name not in TYPE_WORDS:
                    names.setdefault(name, header)

    return names


def compiled_sources() -> set[Path]:
    """The game files the native build compiles: each unit's sources (unit_globals.json), less the files the port
    replaces (its exclude), and the .inc.c files those include. Overlays that aren't native units (the movie player,
    which the port's own player replaces, and CHECKPS, which it skips) aren't compiled at all."""
    settings = json.loads(UNIT_SETTINGS.read_text())
    units = settings.get("units", settings)
    files: set[Path] = set()
    for data in units.values():
        if not isinstance(data, dict):
            continue
        excluded = {ROOT / "game" / name for name in data.get("exclude", [])}
        for pattern in data.get("sources", []):
            files |= {path for path in (ROOT / "game").glob(pattern) if path not in excluded}
    included = set()
    for path in files:
        for match in re.finditer(r'#include "([^"]+\.inc\.c)"', path.read_text(errors="replace")):
            included.add((path.parent / match.group(1)).resolve())
    return {path.resolve() for path in files} | included


def game_headers() -> set[Path]:
    """The game's own headers, whose macros and inline functions are compiled into its files too: game/include
    without the SDK's (sdk/), and the overlays' internal ones, without the decomp's copy of Sony's libraries."""
    include, source = (ROOT / "game/include").resolve(), (ROOT / "game/src").resolve()
    return ({path.resolve() for path in include.rglob("*.h") if path.resolve().relative_to(include).parts[0] != "sdk"}
            | {path.resolve() for path in source.rglob("*.h") if "psyq" not in path.resolve().relative_to(source).parts})


def game_sources() -> list[Path]:
    """The game's own C files that the native build compiles, and its headers, without the decomp's copy of Sony's
    libraries or the original backends' files."""
    source = (ROOT / "game/src").resolve()
    compiled = compiled_sources()
    files = sorted(path for path in compiled
                   if path.is_relative_to(source) and "psyq" not in path.relative_to(source).parts
                   and not any(path.relative_to(source).match(pattern) for pattern in BACKEND_FILES))
    return files + sorted(game_headers())


def side_doors(names: dict[str, str]) -> dict[str, dict[str, int]]:
    """Calls to platform names in the game code, as {file: {name: count}}."""
    pattern = re.compile(r"\b(" + "|".join(sorted(map(re.escape, names), key=len, reverse=True)) + r")\s*\(")
    found: dict[str, dict[str, int]] = {}

    for path in game_sources():
        text = strip_comments_and_strings(path.read_text(errors="replace"))
        counts: collections.Counter[str] = collections.Counter()

        for match in pattern.finditer(text):
            line_start = text.rfind("\n", 0, match.start()) + 1

            before = text[line_start:match.start()]

            if re.match(r"\s*#\s*define\s*$", before) or re.match(r"\s*#\s*(include|if|ifdef|ifndef|elif|undef)\b", before):
                continue

            previous = re.search(r"([A-Za-z_]\w*|\*)\s*$", text[max(0, match.start() - 80) : match.start()])

            if previous and (previous.group(1) in TYPE_WORDS or previous.group(1) == "*" or previous.group(1).endswith("_t")):
                continue

            if previous and previous.group(1)[0].isupper() and re.match(r"^[A-Z][A-Za-z0-9]*$", previous.group(1)) and previous.group(1) not in names:
                continue

            counts[match.group(1)] += 1

        if counts:
            found[str(path.relative_to(ROOT.resolve()))] = dict(sorted(counts.items()))

    return found


def port_game_behavior() -> list[str]:
    """Places in src/ that change the game's state or run its flow, as "file:line what" lines."""
    call = re.compile(r"\b(" + "|".join(GAME_FLOW) + r"|akao_\w+)\s*\(")
    write = re.compile(r"\b(" + "|".join(GAME_STATE) + r")\b(?:\.\w+|\[[^\]]*\]|->\w+)*\s*(?:=(?!=)|\+=|-=|\|=|&=|\^=|\+\+|--)")
    found = []
    for path in sorted([*(ROOT / "src").rglob("*.c"), *(ROOT / "src").rglob("*.cpp")]):
        relative = str(path.relative_to(ROOT))
        allowed = set().union(*(names for pattern, names in PORT_ALLOWED.items() if Path(relative).match(pattern)))
        text = strip_comments_and_strings(path.read_text(errors="replace"))
        for match in call.finditer(text):
            before = text[text.rfind("\n", 0, match.start()) + 1 : match.start()]
            if before.lstrip().startswith("#") or re.search(r"\b(extern|void|s32|int|u32|static|unsigned|long|char)\b[\s*]*$", before):
                continue
            name = match.group(1)
            if name in allowed or (name.startswith("akao_") and "akao_*" in allowed):
                continue
            found.append(f"{relative}:{text.count(chr(10), 0, match.start()) + 1} calls {name}")
        for match in write.finditer(text):
            found.append(f"{relative}:{text.count(chr(10), 0, match.start()) + 1} changes {match.group(1)}")
    return found


def renames(names: dict[str, str]) -> list[str]:
    """Build-time renames of platform names, as "unit pattern name" lines."""
    settings = json.loads(UNIT_SETTINGS.read_text())
    units = settings.get("units", settings)
    found = []

    for unit, data in units.items():
        if not isinstance(data, dict):
            continue

        for pattern, defines in data.get("defines_by_file", {}).items():
            for define in defines:
                name = define.split("=", 1)[0]

                if name in names:
                    found.append(f"{unit} {pattern} {name}")

    return sorted(found)


def main() -> int:
    """Find the game's calls to the platform and the build's renames of them, and compare them with the allow list, or rewrite it.

    @return 0 when they match.
    """
    parser = argparse.ArgumentParser(description=DESCRIPTION.split("\n\n")[0])
    parser.add_argument("--update", action="store_true", help="rewrite the allow list from what's there now")
    parser.add_argument("--report", action="store_true", help="print totals by library and by unit")
    args = parser.parse_args()

    names = platform_names()
    calls = side_doors(names)
    renamed = renames(names)

    if args.report:
        by_header: collections.Counter[str] = collections.Counter()
        by_unit: collections.Counter[str] = collections.Counter()

        for file, counts in calls.items():
            parts = Path(file).parts
            unit = parts[3] if parts[2] == "overlays" else parts[2]

            for name, count in counts.items():
                by_header[names[name]] += count
                by_unit[unit] += count

        print(f"side doors: {sum(by_header.values())} calls in {len(calls)} files; renames: {len(renamed)}")
        print("by library: " + ", ".join(f"{header} {count}" for header, count in by_header.most_common()))
        print("by unit: " + ", ".join(f"{unit} {count}" for unit, count in by_unit.most_common()))
        return 0

    if args.update:
        ALLOW.write_text(json.dumps({
            "about": "What the game still does around the platform boundaries: calls to PlayStation library "
                     "and kernel functions in game/src, and build-time renames of them. tools/testing/separation_check.py checks it "
                     "matches exactly; it should shrink to nothing.",
            "calls": calls,
            "renames": renamed,
        }, indent=1) + "\n")
        print(f"separation: wrote {ALLOW.relative_to(ROOT)}: {sum(sum(c.values()) for c in calls.values())} calls in {len(calls)} files, {len(renamed)} renames")
        return 0

    allow = json.loads(ALLOW.read_text())
    problems = []
    allowed_calls = allow.get("calls", {})

    for file in sorted(set(calls) | set(allowed_calls)):
        have, allowed = calls.get(file, {}), allowed_calls.get(file, {})

        for name in sorted(set(have) | set(allowed)):
            if have.get(name, 0) > allowed.get(name, 0):
                problems.append(f"new: {file} calls {name} {have.get(name, 0)} times, the allow list says {allowed.get(name, 0)}")
            elif have.get(name, 0) < allowed.get(name, 0):
                problems.append(f"gone: {file} calls {name} {have.get(name, 0)} times, the allow list still says {allowed.get(name, 0)} (run --update)")

    for line in sorted(set(renamed) - set(allow.get("renames", []))):
        problems.append(f"new rename: {line}")

    for line in sorted(set(allow.get("renames", [])) - set(renamed)):
        problems.append(f"gone rename: {line} is still in the allow list (run --update)")

    in_src = port_game_behavior()

    for line in in_src:
        problems.append(f"game behavior in src/: {line}")

    for problem in problems:
        print(problem)

    total = sum(sum(c.values()) for c in calls.values())
    print(f"separation: {total} platform calls in {len(calls)} game files, {len(renamed)} renames, {len(in_src)} places in src/ doing the game's work; "
          + ("matches the allow list" if not problems else f"{len(problems)} differences from the allow list"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
import argparse
import difflib
import shutil
import sys
from pathlib import Path

DESCRIPTION = """Check the game's globals are laid out the way the repo says they are.

    python3 tools/build/check_unit_layout.py BUILD/generated/units/layout.txt
    python3 tools/build/check_unit_layout.py BUILD/generated/units/layout.txt --accept
    python3 tools/build/check_unit_layout.py BUILD/generated/units-jp/layout.txt --version jp

generate_unit_globals.py decides how each of the game's globals is defined
natively: under its own type, or as raw storage at its PS1 size, and in what
order. That decides where each one sits in memory, and it can change without
any game code changing. Moving a declaration from a .c file to a header is
enough, and once that moved the music sequencer under a cutscene that writes
past the end of its save data.

The generator writes what it decided to layout.txt, one line per global: its
PS1 address and size, then its definition. This compares that with
tools/build/unit_globals_layout.txt and stops the build when they differ,
showing what changed. When the change is meant, --accept copies the new
layout over the checked-in one, so it goes into the same commit and the
review sees it.

The Japanese game (--version jp) has its own layout,
tools/build/unit_globals_layout_jp.txt.
"""

REPO = Path(__file__).resolve().parents[2]
EXPECTED = {'us': REPO / 'tools/build/unit_globals_layout.txt', 'jp': REPO / 'tools/build/unit_globals_layout_jp.txt'}
DIFF_LINES = 60


def read_lines(path):
    """Read a layout file's lines, ignoring line endings.

    @param path The file.
    @return Its lines, without their endings; empty when it doesn't exist.
    """
    if not path.exists():
        return []
    return path.read_text().splitlines()


def main():
    """Compare the build's layout with the checked-in one, or accept it.

    @return 0 when they match or the new one was accepted, 1 when they differ.
    """
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('layout', type=Path, help="the layout.txt the generator wrote in the build's units folder")
    parser.add_argument('--version', choices=sorted(EXPECTED), default='us', help='the game it was built for: us (the default) or jp')
    parser.add_argument('--accept', action='store_true', help="copy it over the checked-in layout")
    args = parser.parse_args()
    checked_in = EXPECTED[args.version]
    name = checked_in.relative_to(REPO)

    if args.accept:
        shutil.copyfile(args.layout, checked_in)
        print(f'{name} updated; commit it with the change that moved the globals')
        return 0

    expected, actual = read_lines(checked_in), read_lines(args.layout)
    if expected == actual:
        print(f"the game's globals are laid out as {name} says")
        return 0

    diff = list(difflib.unified_diff(expected, actual, checked_in.name, 'this build', lineterm='', n=1))
    shown = diff[:DIFF_LINES]
    if len(diff) > DIFF_LINES:
        shown.append(f'... and {len(diff) - DIFF_LINES} more lines')
    print('\n'.join(shown), file=sys.stderr)
    print(f"\nThe game's globals moved in memory. If that's meant, accept it:\n"
          f'    python3 tools/build/check_unit_layout.py {args.layout} --version {args.version} --accept', file=sys.stderr)
    return 1


if __name__ == '__main__':
    sys.exit(main())

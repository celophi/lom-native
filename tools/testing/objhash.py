#!/usr/bin/env python3
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

DESCRIPTION = """Fingerprint every object a build compiles, to prove an edit didn't change it.

    objhash.py BUILD OUT.json             write a fingerprint of BUILD
    objhash.py --compare BEFORE.json AFTER.json

The fingerprint of an object is a hash of its disassembly, with relocations
and without raw bytes or addresses, so the build folder's path and debug info
don't count. Take one before an edit to game/, rebuild, take another, and
compare: only the objects you meant to change may differ.

Check the build actually recompiled before trusting "0 differ".
"""


def object_path(build: Path, entry: dict) -> Path:
    """Where a compile_commands.json entry writes its object.

    CMake's "output" is relative to the top of the build folder, not to the
    entry's own directory.
    """
    output = entry.get("output")
    if output is None:
        arguments = entry.get("arguments") or shlex.split(entry["command"])
        output = arguments[arguments.index("-o") + 1]
        path = Path(output)
        return path if path.is_absolute() else Path(entry["directory"]) / path
    path = Path(output)
    return path if path.is_absolute() else build / path


def fingerprint(path: Path) -> str:
    """A hash of an object's disassembly, or "missing" if it isn't built."""
    if not path.exists():
        return "missing"
    disassembly = subprocess.run(["objdump", "-d", "-r", "--no-show-raw-insn", str(path)], capture_output=True, text=True, check=True).stdout
    lines = [re.sub(r"^\s*[0-9a-f]+:\s*", "", line) for line in disassembly.splitlines()[3:]]
    return hashlib.sha1("\n".join(lines).encode()).hexdigest()


def write(build: Path, out: Path) -> None:
    """Fingerprint the disassembly of every object a build compiles.

    @param build The build's folder.
    @param out Where to write the fingerprints.
    """
    build = build.resolve()
    entries = json.loads((build / "compile_commands.json").read_text())
    paths = sorted({path for path in (object_path(build, entry) for entry in entries) if path.suffix == ".o"})
    with ThreadPoolExecutor() as pool:
        hashes = dict(zip((str(path.relative_to(build)) if path.is_relative_to(build) else str(path) for path in paths), pool.map(fingerprint, paths)))
    out.write_text(json.dumps(hashes, indent=0, sort_keys=True) + "\n")
    print(f"{len(hashes)} objects, {sum(value == 'missing' for value in hashes.values())} not built")


def compare(before: Path, after: Path) -> int:
    """Say which objects differ between two fingerprints.

    @param before The earlier fingerprints.
    @param after The later ones.
    @return 0 when none differ, otherwise 1.
    """
    old, new = json.loads(before.read_text()), json.loads(after.read_text())
    changed = sorted(name for name in old.keys() & new.keys() if old[name] != new[name])
    for name in changed:
        print(f"differs  {name}")
    for name in sorted(old.keys() - new.keys()):
        print(f"gone     {name}")
    for name in sorted(new.keys() - old.keys()):
        print(f"new      {name}")
    print(f"{len(changed)} differ, {len(old.keys() - new.keys())} gone, {len(new.keys() - old.keys())} new")
    return 1 if changed else 0


def main() -> int:
    """Write a build's fingerprints, or compare two.

    @return 0, or 1 when they differ.
    """
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--compare", action="store_true", help="compare two fingerprints instead of writing one")
    parser.add_argument("first", type=Path, help="the build folder, or the earlier fingerprint")
    parser.add_argument("second", type=Path, help="where to write the fingerprint, or the later one")
    args = parser.parse_args()
    if args.compare:
        return compare(args.first, args.second)
    write(args.first, args.second)
    return 0


if __name__ == "__main__":
    sys.exit(main())

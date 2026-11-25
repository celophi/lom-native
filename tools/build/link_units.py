#!/usr/bin/env python3
from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path
import subprocess
import sys

import coff_units
import macho_wrap

DESCRIPTION = """Link each unit of the game into one object, keeping units apart.

    link_units.py OUT_DIR UNIT=OBJECT_LIST... [--ld LD] [--nm NM] [--objcopy OBJCOPY] [--wrap NAME]...

On the PS1 every overlay is linked on its own, so two overlays can use the
same name: FIELD's sub-overlays each have their own copy of the card menu
code and its data. The build links everything into one program, so
this gives every unit the same separation:

1. Each unit's objects are linked into one (ld -r), so references inside a
   unit bind there.
2. A name that more than one unit defines is made local in each of them.
   Weak definitions (the shared list's globals, which are one object on
   purpose) are left alone.
3. If another unit, or the port, needs one of those names, it can't tell which
   unit's it means, so that's an error.

OBJECT_LIST is a file with one object path per line. The result is
OUT_DIR/UNIT.o for each unit.

--wrap NAME passes ld's --wrap to step 1, for a test program that watches
NAME (the route runner). The final link's --wrap only sees calls that cross
objects, and step 1 already bound the ones inside a unit, so this makes a
unit's calls between its own files go through __wrap_NAME too. Calls within
one file aren't wrapped.

On macOS the objects are Mach-O and the linker is Apple's, which has no --wrap
and no objcopy. There, macho_wrap.py renames the unit's objects for --wrap
before step 1, and step 2 is a second ld -r that hides the names
(-unexported_symbols_list), which makes them local the same way.

On Windows the objects are COFF, and step 1 is GNU ld (LLD has no ld -r for
COFF). coff_units.py then does step 2 and repairs the weak symbols GNU ld
leaves broken. A weak reference there needs to know what the rest of the
program defines: the other units' names, and --available LIBRARY for each
library linked with them (the port, PSY-Z).
"""


def run(*command: str) -> str:
    """Run a command, stopping with its errors when it fails.

    @param command The command and its arguments.
    @return What it printed.
    """
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"link_units: {command[0]} failed:\n{result.stderr}")
    return result.stdout


EMULATIONS = {(1, 3): "elf_i386", (2, 62): "elf_x86_64", (1, 40): "armelf_linux_eabi", (2, 183): "aarch64linux"}


def emulation(path: str) -> list[str]:
    """ld's -m option for the format of an ELF object, or nothing if it isn't one ld here knows."""
    with open(path, "rb") as file:
        header = file.read(20)
    if header[:4] != b"\x7fELF":
        return []
    machine = int.from_bytes(header[18:20], "little" if header[5] == 1 else "big")
    found = EMULATIONS.get((header[4], machine))
    return ["-m", found] if found else []


def is_macho(path: str) -> bool:
    """Whether an object is Mach-O (a macOS build)."""
    with open(path, "rb") as file:
        return macho_wrap.is_macho_object(file.read(macho_wrap.MACHO_HEADER.size))


def wrapped_copies(objects: list[str], folder: Path, names: list[str]) -> list[str]:
    """Copies of a unit's Mach-O objects with --wrap's renames for names, numbered so names can't collide."""
    folder.mkdir(parents=True, exist_ok=True)
    copies = []
    for index, path in enumerate(objects):
        copy = folder / f"{index:05d}_{Path(path).name}"
        macho_wrap.wrap_object(Path(path), copy, names)
        copies.append(str(copy))
    return copies


def symbols(nm: str, path: Path) -> tuple[set[str], set[str], set[str]]:
    """Strong definitions, weak definitions and undefined names in an object."""
    if is_macho(str(path)):
        kinds = macho_wrap.external_symbols(path.read_bytes())
        strong = {name for name, kind in kinds.items() if kind == "strong"}
        weak = {name for name, kind in kinds.items() if kind in ("weak", "weak reference")}
        undefined = {name for name, kind in kinds.items() if kind == "undefined"}
        return strong, weak, undefined
    coff = coff_units.is_coff_object(path)
    strong, weak, undefined = set(), set(), set()
    for line in run(nm, "-g", "--format=posix", str(path)).splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        name, kind = parts[0], parts[1]
        if kind == "U":
            undefined.add(name)
        elif kind in "VWvw":
            weak.add(name)
        elif coff and coff_units.fallback_name(name) is not None:
            continue
        else:
            strong.add(name)
    return strong, weak, undefined - strong - weak


def library_definitions(nm: str, libraries: list[str]) -> set[str]:
    """Every name the libraries define."""
    names = set()
    for library in libraries:
        for line in run(nm, "-g", "--defined-only", "--format=posix", library).splitlines():
            parts = line.split()
            if len(parts) >= 2 and not line.endswith(":"):
                names.add(parts[0])
    return names


def main() -> int:
    """Link each of the game's units into one object, keeping only the names other units use.

    @return 0, or 1 when two units define a name a third one uses.
    """
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("units", nargs="+", help="UNIT=OBJECT_LIST")
    parser.add_argument("--ld", default="ld")
    parser.add_argument("--nm", default="nm")
    parser.add_argument("--objcopy", default="objcopy")
    parser.add_argument("--wrap", action="append", default=[], help="ld --wrap for step 1")
    parser.add_argument("--available", action="append", default=[], help="a library linked with the units (Windows)")
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)

    partial: dict[str, Path] = {}
    for spec in args.units:
        name, _, listing = spec.partition("=")
        objects = [line for line in Path(listing).read_text().split() if line]
        partial[name] = args.out_dir / f"{name}.partial.o"
        if is_macho(objects[0]):
            if args.wrap:
                objects = wrapped_copies(objects, args.out_dir / f"{name}.wrapped", args.wrap)
            run(args.ld, "-r", "-o", str(partial[name]), *objects)
        else:
            run(args.ld, *emulation(objects[0]), "-r", *(f"--wrap={symbol}" for symbol in args.wrap), "-o", str(partial[name]), *objects)

    strong: dict[str, set[str]] = {}
    undefined: dict[str, set[str]] = {}
    definers: dict[str, list[str]] = defaultdict(list)
    for name, path in partial.items():
        strong[name], _weak, undefined[name] = symbols(args.nm, path)
        for symbol in strong[name]:
            definers[symbol].append(name)

    clashes = {symbol: units for symbol, units in definers.items() if len(units) > 1}
    problems = []
    for name in partial:
        for symbol in sorted(undefined[name] & clashes.keys()):
            if name not in clashes[symbol]:
                problems.append(f"{name} uses {symbol}, which {', '.join(clashes[symbol])} all define")
    if problems:
        print("link_units: " + "\n  ".join(["names more than one unit defines:", *problems]), file=sys.stderr)
        return 1

    outside = None
    for name, path in partial.items():
        local = sorted(strong[name] & clashes.keys())
        out = args.out_dir / f"{name}.o"
        if coff_units.is_coff_object(path):
            if outside is None:
                outside = library_definitions(args.nm, args.available)
                for unit_names in strong.values():
                    outside |= unit_names - clashes.keys()
            data = bytearray(path.read_bytes())
            coff_units.finish(data, set(local), outside - strong[name])
            out.write_bytes(bytes(data))
        elif local and is_macho(str(path)):
            listing = args.out_dir / f"{name}.local.txt"
            listing.write_text("".join(f"{macho_wrap.SYMBOL_PREFIX}{symbol}\n" for symbol in local))
            run(args.ld, "-r", "-unexported_symbols_list", str(listing), "-o", str(out), str(path))
        elif local:
            listing = args.out_dir / f"{name}.local.txt"
            listing.write_text("\n".join(local) + "\n")
            run(args.objcopy, f"--localize-symbols={listing}", str(path), str(out))
        else:
            out.write_bytes(path.read_bytes())
    print(f"link_units: {len(partial)} units, {len(clashes)} names kept apart")
    return 0


if __name__ == "__main__":
    sys.exit(main())

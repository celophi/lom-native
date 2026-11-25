#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import struct
import sys

DESCRIPTION = """Finish a Windows (COFF) unit after GNU ld's partial link (link_units.py).

    coff_units.py IN OUT [--local FILE] [--available FILE]

LLD can't do a partial link of COFF objects, so on Windows link_units.py uses
GNU ld for it. Two things then need fixing, both in place, without moving
anything in the object:

1. Weak symbols. LLVM writes a weak symbol NAME as a "weak external" that
   points, by symbol number, at a fallback named .weak.NAME.default.SOMETHING:
   the definition for a weak definition, or an absolute 0 for a weak
   reference. GNU ld renumbers the symbol table and doesn't update those
   numbers, so LLD can't find the fallbacks. Each one is found again by name.

   Weak references (the units' function tables, by PS1 address) also need
   ELF's meaning: the function the unit defines, else one defined anywhere
   else in the program, else NULL. GNU ld leaves them unbound, and LLD won't
   load a library member for a name that has a weak reference, so a table
   entry for the PS1's __main or write would stop the C runtime's from being
   linked at all. So each weak reference becomes one of:
   - a reference to the unit's own definition, if it has one;
   - a plain reference, if --available names it (defined by another unit,
     the port or PSY-Z);
   - the fallback, NULL, with its name taken out of the program's names.
   A weak reference ld's --wrap renamed to __wrap_NAME keeps NAME's fallback
   and becomes a plain reference: the program that asked for the wrap
   defines __wrap_NAME.

2. Names more than one unit defines (--local) become local, the way
   objcopy --localize-symbols does on Linux. GNU objcopy would renumber the
   symbol table again, so this flips their storage class here instead.

--local and --available are files with one name per line.
"""

FILE_HEADER = struct.Struct("<HHIIIHH")
SECTION = struct.Struct("<8sIIIIIIHHI")
SYMBOL = struct.Struct("<8sIhHBB")
RELOCATION = struct.Struct("<IIH")
WEAK_AUX = struct.Struct("<II")

MACHINES = {0x8664, 0x14C, 0xAA64}
CLASS_EXTERNAL = 2
CLASS_STATIC = 3
CLASS_WEAK_EXTERNAL = 105
SECTION_UNDEFINED = 0
SECTION_ABSOLUTE = -1
RELOCATIONS_OVERFLOW = 0x01000000
WEAK_PREFIX = ".weak."
DEFAULT_MARK = ".default."
WRAP_PREFIX = "__wrap_"


def is_coff_object(path: Path) -> bool:
    """Whether a file is a (not big) COFF object for a machine the port builds for."""
    with open(path, "rb") as file:
        header = file.read(FILE_HEADER.size)
    return len(header) == FILE_HEADER.size and FILE_HEADER.unpack(header)[0] in MACHINES


class CoffObject:
    def __init__(self, data: bytearray):
        """Read a COFF object's header, sections and symbol table.

        @param data The object's bytes, changed in place.
        """
        self.data = data
        machine, section_count, _, self.symbol_table, self.symbol_count, optional_size, _ = FILE_HEADER.unpack_from(data)
        if machine not in MACHINES:
            raise ValueError("not a COFF object")
        self.strings = self.symbol_table + self.symbol_count * SYMBOL.size
        self.relocation_tables = []
        for index in range(section_count):
            fields = SECTION.unpack_from(data, FILE_HEADER.size + optional_size + index * SECTION.size)
            pointer, count, characteristics = fields[5], fields[7], fields[9]
            if characteristics & RELOCATIONS_OVERFLOW and count == 0xFFFF:
                count = RELOCATION.unpack_from(data, pointer)[0] - 1
                pointer += RELOCATION.size
            if count:
                self.relocation_tables.append((pointer, count))
        self.symbols: dict[int, tuple[str, int, int, int, int]] = {}
        index = 0
        while index < self.symbol_count:
            raw, value, section, _type, storage, aux = SYMBOL.unpack_from(data, self.symbol_offset(index))
            self.symbols[index] = (self.symbol_name(raw), value, section, storage, aux)
            index += 1 + aux

    def symbol_offset(self, index: int) -> int:
        """Find a symbol in the symbol table.

        @param index The symbol's number.
        @return Its offset in the object.
        """
        return self.symbol_table + index * SYMBOL.size

    def symbol_name(self, raw: bytes) -> str:
        """Read a symbol's name, short or from the string table.

        @param raw The symbol's name field.
        @return The name.
        """
        if raw[:4] == b"\0\0\0\0":
            start = self.strings + struct.unpack_from("<I", raw, 4)[0]
            end = self.data.index(b"\0", start)
            return self.data[start:end].decode("utf-8", "replace")
        return raw.rstrip(b"\0").decode("utf-8", "replace")

    def set_symbol(self, index: int, section: int, storage: int) -> None:
        """Change a symbol's section number and storage class, keeping the rest."""
        offset = self.symbol_offset(index)
        raw, value, _section, type_, _storage, aux = SYMBOL.unpack_from(self.data, offset)
        SYMBOL.pack_into(self.data, offset, raw, value, section, type_, storage, aux)

    def set_weak_fallback(self, index: int, fallback: int) -> None:
        """Point a weak external at its fallback symbol, keeping its search kind."""
        offset = self.symbol_offset(index + 1)
        _tag, characteristics = WEAK_AUX.unpack_from(self.data, offset)
        WEAK_AUX.pack_into(self.data, offset, fallback, characteristics)

    def retarget(self, old: int, new: int) -> int:
        """Make every relocation against symbol old use symbol new. Returns how many moved."""
        moved = 0
        for pointer, count in self.relocation_tables:
            for offset in range(pointer, pointer + count * RELOCATION.size, RELOCATION.size):
                address, symbol, type_ = RELOCATION.unpack_from(self.data, offset)
                if symbol == old:
                    RELOCATION.pack_into(self.data, offset, address, new, type_)
                    moved += 1
        return moved


def fallback_name(name: str) -> str | None:
    """NAME for a weak external's fallback .weak.NAME.default.SOMETHING, or None."""
    if not name.startswith(WEAK_PREFIX) or DEFAULT_MARK not in name:
        return None
    return name[len(WEAK_PREFIX):name.rindex(DEFAULT_MARK)]


def finish(data: bytearray, local: set[str], available: set[str]) -> dict[str, int]:
    """Fix the weak symbols and make the local names local. Returns counts of what changed."""
    coff = CoffObject(data)
    fallbacks: dict[str, int] = {}
    definitions: dict[str, int] = {}
    weak: list[tuple[int, str]] = []
    for index, (name, _value, section, storage, _aux) in coff.symbols.items():
        target = fallback_name(name)
        if target is not None:
            fallbacks.setdefault(target, index)
        elif storage == CLASS_WEAK_EXTERNAL:
            weak.append((index, name))
        elif storage == CLASS_EXTERNAL and section > 0:
            definitions.setdefault(name, index)

    counts = {"weak definitions": 0, "own": 0, "elsewhere": 0, "null": 0, "local": 0}
    for index, name in weak:
        fallback = fallbacks.get(name)
        if fallback is None and name.startswith(WRAP_PREFIX):
            fallback = fallbacks.get(name[len(WRAP_PREFIX):])
        if fallback is None:
            raise SystemExit(f"coff_units: no fallback for weak symbol {name}")
        if coff.symbols[fallback][2] > 0:
            coff.set_weak_fallback(index, fallback)
            counts["weak definitions"] += 1
        elif name in definitions:
            coff.retarget(index, definitions[name])
            coff.set_weak_fallback(index, fallback)
            coff.set_symbol(index, SECTION_ABSOLUTE, CLASS_STATIC)
            counts["own"] += 1
        elif name in available or name.startswith(WRAP_PREFIX):
            coff.set_symbol(index, SECTION_UNDEFINED, CLASS_EXTERNAL)
            counts["elsewhere"] += 1
        else:
            coff.retarget(index, fallback)
            coff.set_weak_fallback(index, fallback)
            coff.set_symbol(index, SECTION_ABSOLUTE, CLASS_STATIC)
            counts["null"] += 1

    for name in local:
        index = definitions.get(name)
        if index is not None:
            coff.set_symbol(index, coff.symbols[index][2], CLASS_STATIC)
            counts["local"] += 1
    return counts


def read_names(path: Path | None) -> set[str]:
    """Read a file of names, one per line.

    @param path The file, or None for none.
    @return The names.
    """
    return set(path.read_text().split()) if path else set()


def main() -> int:
    """Fix a Windows unit's weak symbols and names after GNU ld's partial link, then write it.

    @return 0.
    """
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--local", type=Path, help="names to make local, one per line")
    parser.add_argument("--available", type=Path, help="names defined outside this unit, one per line")
    args = parser.parse_args()
    data = bytearray(args.input.read_bytes())
    counts = finish(data, read_names(args.local), read_names(args.available))
    args.output.write_bytes(bytes(data))
    print(f"coff_units: {args.output.name}: " + ", ".join(f"{count} {kind}" for kind, count in counts.items()))
    return 0


if __name__ == "__main__":
    sys.exit(main())

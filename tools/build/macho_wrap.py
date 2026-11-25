#!/usr/bin/env python3
from __future__ import annotations

import hashlib
from pathlib import Path
import shlex
import struct
import subprocess
import sys

DESCRIPTION = """Two things GNU ld does that Apple's linker doesn't: --wrap, and weak references.

    macho_wrap.py LINKER [ARGS...]          run a link, doing both first
    macho_wrap.py --object IN OUT NAME...   wrap NAME... in one object

On macOS CMake runs every link through this (its linker launcher).

Some tests swap a function for their own with ld's --wrap=NAME: every
undefined reference to NAME goes to __wrap_NAME instead, and __real_NAME goes
to the real NAME. Apple's linker doesn't have it. This takes the --wrap
options out of the command, renames those references in copies of the
objects and static libraries the link reads, and runs the link on the copies.
Only undefined names are renamed, the same as --wrap: a call inside the file
that defines NAME isn't wrapped. link_units.py uses wrap_object() for its own
--wrap.

The port also declares some functions weak, so a program that doesn't have
them gets NULL (only the tests define the test clock's hooks, say). GNU ld
leaves a missing weak reference at zero. Apple's linker stops, unless each one
is named with -U. So this finds every weak reference in the link's objects and
libraries and names it. One that's defined still binds as normal.
"""

MACHO_MAGIC = 0xFEEDFACF
MACHO_HEADER = struct.Struct("<IiiIIIII")
LOAD_COMMAND = struct.Struct("<II")
LC_SYMTAB = 0x2
SYMTAB_COMMAND = struct.Struct("<IIIIII")
NLIST = struct.Struct("<IBBHQ")
N_STAB = 0xE0
N_TYPE = 0x0E
N_UNDF = 0x0
N_EXT = 0x01
N_WEAK_DEF = 0x0080
N_WEAK_REF = 0x0040
SYMBOL_PREFIX = "_"

ARCHIVE_MAGIC = b"!<arch>\n"
ARCHIVE_MEMBER = struct.Struct("16s12s6s6s8s10s2s")
LONG_NAME = "#1/"
SYMBOL_TABLES = ("__.SYMDEF", "__.SYMDEF SORTED")

DRIVER_LINKER_PREFIX = "-Wl,"
WRAP_OPTION = "--wrap="
MAY_BE_UNDEFINED = "-U"
COPIES_SUFFIX = ".wrapped"


def renames(names: list[str]) -> dict[bytes, bytes]:
    """The Mach-O renames --wrap makes for each C name."""
    table: dict[bytes, bytes] = {}
    for name in names:
        table[f"{SYMBOL_PREFIX}{name}".encode()] = f"{SYMBOL_PREFIX}__wrap_{name}".encode()
        table[f"{SYMBOL_PREFIX}__real_{name}".encode()] = f"{SYMBOL_PREFIX}{name}".encode()
    return table


def is_macho_object(data: bytes) -> bool:
    """Whether data is a 64-bit little-endian Mach-O file."""
    return len(data) >= MACHO_HEADER.size and struct.unpack_from("<I", data)[0] == MACHO_MAGIC


def symbol_table(data: bytes) -> tuple[int | None, int, int, int, int]:
    """Where a Mach-O object's LC_SYMTAB command is, and its symbol and string table offsets and sizes."""
    header = MACHO_HEADER.unpack_from(data)
    offset = MACHO_HEADER.size
    for _ in range(header[4]):
        command, size = LOAD_COMMAND.unpack_from(data, offset)
        if command == LC_SYMTAB:
            _command, _size, symoff, nsyms, stroff, strsize = SYMTAB_COMMAND.unpack_from(data, offset)
            return offset, symoff, nsyms, stroff, strsize
        offset += size
    return None, 0, 0, 0, 0


def external_symbols(data: bytes) -> dict[str, str]:
    """A Mach-O object's external names, as C names, each with its kind.

    The kinds are "strong" and "weak" for definitions, "weak reference" for
    a name it uses weakly, and "undefined" for one it needs.
    """
    _at, symoff, nsyms, stroff, _strsize = symbol_table(data)
    kinds = {}
    for index in range(nsyms):
        strx, kind, _sect, desc, value = NLIST.unpack_from(data, symoff + index * NLIST.size)
        if kind & N_STAB or not kind & N_EXT:
            continue
        name = data[stroff + strx:data.index(b"\0", stroff + strx)].decode()
        name = name[len(SYMBOL_PREFIX):] if name.startswith(SYMBOL_PREFIX) else name
        if kind & N_TYPE == N_UNDF and not value:
            kinds[name] = "weak reference" if desc & N_WEAK_REF else "undefined"
        else:
            kinds[name] = "weak" if desc & N_WEAK_DEF else "strong"
    return kinds


def file_symbols(path: Path) -> dict[str, str]:
    """external_symbols() for a Mach-O object, or for every member of a static library."""
    data = path.read_bytes()
    if not data.startswith(ARCHIVE_MAGIC):
        return external_symbols(data) if is_macho_object(data) else {}
    kinds: dict[str, str] = {}
    for _name, body in archive_members(data):
        if is_macho_object(body):
            for name, kind in external_symbols(body).items():
                if kinds.get(name) not in ("strong", "weak"):
                    kinds[name] = kind
    return kinds


def weak_references(data: bytes) -> set[str]:
    """The names a Mach-O object refers to weakly and doesn't define, as the linker spells them."""
    _at, symoff, nsyms, stroff, _strsize = symbol_table(data)
    names = set()
    for index in range(nsyms):
        strx, kind, _sect, desc, value = NLIST.unpack_from(data, symoff + index * NLIST.size)
        if kind & N_STAB or kind & N_TYPE != N_UNDF or not kind & N_EXT or value or not desc & N_WEAK_REF:
            continue
        names.add(data[stroff + strx:data.index(b"\0", stroff + strx)].decode())
    return names


def rename_undefined(data: bytes, table: dict[bytes, bytes]) -> bytes | None:
    """A copy of a Mach-O object with its undefined names renamed, or None if none match.

    New names go on the end of the string table. The table normally ends the
    file; if it doesn't, it's moved there whole. Nothing else in an object
    points into it, so that's all a rename needs.
    """
    symtab_at, symoff, nsyms, stroff, strsize = symbol_table(data)
    if symtab_at is None:
        return None

    strings = bytearray(data[stroff:stroff + strsize])
    out = bytearray(data)
    changed = False
    for index in range(nsyms):
        at = symoff + index * NLIST.size
        strx, kind, sect, desc, value = NLIST.unpack_from(data, at)
        if kind & N_STAB or kind & N_TYPE != N_UNDF or not kind & N_EXT or value:
            continue
        end = data.index(b"\0", stroff + strx)
        new = table.get(data[stroff + strx:end])
        if new is None:
            continue
        NLIST.pack_into(out, at, len(strings), kind, sect, desc, value)
        strings += new + b"\0"
        changed = True
    if not changed:
        return None

    while len(strings) % 8:
        strings += b"\0"
    if stroff + strsize == len(data):
        new_stroff = stroff
    else:
        new_stroff = (len(out) + 7) & ~7
    del out[new_stroff:]
    out += b"\0" * (new_stroff - len(out))
    out += strings
    SYMTAB_COMMAND.pack_into(out, symtab_at, LC_SYMTAB, SYMTAB_COMMAND.size, symoff, nsyms, new_stroff, len(strings))
    return bytes(out)


def wrap_object(source: Path, target: Path, names: list[str]) -> None:
    """Write source to target with --wrap's renames for names."""
    data = source.read_bytes()
    renamed = rename_undefined(data, renames(names)) if is_macho_object(data) else None
    target.write_bytes(renamed if renamed is not None else data)


def archive_members(data: bytes) -> list[tuple[str, bytes]]:
    """A BSD archive's members, in order, without its symbol table."""
    members = []
    offset = len(ARCHIVE_MAGIC)
    while offset + ARCHIVE_MEMBER.size <= len(data):
        fields = ARCHIVE_MEMBER.unpack_from(data, offset)
        name = fields[0].decode().rstrip()
        size = int(fields[5].decode().strip())
        body = data[offset + ARCHIVE_MEMBER.size:offset + ARCHIVE_MEMBER.size + size]
        if name.startswith(LONG_NAME):
            length = int(name[len(LONG_NAME):])
            name, body = body[:length].rstrip(b"\0").decode(), body[length:]
        if name not in SYMBOL_TABLES:
            members.append((name, body))
        offset += ARCHIVE_MEMBER.size + size + (size & 1)
    return members


def wrap_archive(source: Path, folder: Path, table: dict[bytes, bytes]) -> Path | None:
    """A copy of a static library with its members' undefined names renamed, or None if none match.

    The members are written out under numbered names, so two with the same name
    stay apart, and libtool builds the library again in the same order.
    """
    members = archive_members(source.read_bytes())
    renamed = [rename_undefined(body, table) if is_macho_object(body) else None for _name, body in members]
    if all(body is None for body in renamed):
        return None
    folder.mkdir(parents=True, exist_ok=True)
    paths = []
    for index, ((name, body), new) in enumerate(zip(members, renamed)):
        path = folder / f"{index:05d}_{Path(name).name}"
        path.write_bytes(new if new is not None else body)
        paths.append(str(path))
    library = folder.with_suffix(".a")
    subprocess.run(["libtool", "-static", "-no_warning_for_no_symbols", "-o", str(library), *paths], check=True)
    return library


def expand_response_files(arguments: list[str]) -> list[str]:
    """The arguments with each @file replaced by what's in it."""
    expanded = []
    for argument in arguments:
        if argument.startswith("@") and Path(argument[1:]).is_file():
            expanded += shlex.split(Path(argument[1:]).read_text())
        else:
            expanded.append(argument)
    return expanded


def take_wraps(arguments: list[str]) -> tuple[list[str], list[str]]:
    """The command without its --wrap options, and the names they wrap."""
    names, kept = [], []
    for argument in arguments:
        if not argument.startswith(DRIVER_LINKER_PREFIX):
            kept.append(argument)
            continue
        options = argument[len(DRIVER_LINKER_PREFIX):].split(",")
        names += [option[len(WRAP_OPTION):] for option in options if option.startswith(WRAP_OPTION)]
        rest = [option for option in options if not option.startswith(WRAP_OPTION)]
        if rest:
            kept.append(DRIVER_LINKER_PREFIX + ",".join(rest))
    return kept, names


def output_of(arguments: list[str]) -> Path:
    """The file a link writes (its -o)."""
    return Path(arguments[arguments.index("-o") + 1])


def wrap_inputs(arguments: list[str], names: list[str]) -> list[str]:
    """The link's arguments with each object and static library it reads swapped for a renamed copy."""
    table = renames(names)
    output = output_of(arguments)
    folder = output.parent / (output.name + COPIES_SUFFIX)
    folder.mkdir(parents=True, exist_ok=True)
    result = []
    for argument in arguments:
        path = Path(argument)
        if argument.endswith(".o") and path.is_file() and path != output:
            data = path.read_bytes()
            renamed = rename_undefined(data, table) if is_macho_object(data) else None
            if renamed is not None:
                copy = folder / (hashlib.sha1(argument.encode()).hexdigest()[:12] + "_" + path.name)
                copy.write_bytes(renamed)
                argument = str(copy)
        elif argument.endswith(".a") and path.is_file():
            copy = wrap_archive(path, folder / (hashlib.sha1(argument.encode()).hexdigest()[:12] + "_" + path.stem), table)
            if copy is not None:
                argument = str(copy)
        result.append(argument)
    return result


def input_weak_references(arguments: list[str]) -> set[str]:
    """Every weak reference in the objects and static libraries a link reads."""
    names: set[str] = set()
    for argument in arguments:
        path = Path(argument)
        if argument.endswith(".o") and path.is_file():
            data = path.read_bytes()
            if is_macho_object(data):
                names |= weak_references(data)
        elif argument.endswith(".a") and path.is_file():
            for _name, body in archive_members(path.read_bytes()):
                if is_macho_object(body):
                    names |= weak_references(body)
    return names


def link(command: list[str]) -> int:
    """Run a link command, doing its --wrap options first and letting weak references stay undefined."""
    arguments, names = take_wraps(expand_response_files(command))
    if names:
        arguments = wrap_inputs(arguments, names)
    weak = input_weak_references(arguments)
    if weak:
        arguments += [DRIVER_LINKER_PREFIX + ",".join(f"{MAY_BE_UNDEFINED},{name}" for name in sorted(weak))]
    return subprocess.run(arguments).returncode


def main() -> int:
    """Wrap one object's names, or run a link with the wraps applied, for macOS's linker, which has no --wrap.

    @return The link's exit code.
    """
    if len(sys.argv) >= 4 and sys.argv[1] == "--object":
        wrap_object(Path(sys.argv[2]), Path(sys.argv[3]), sys.argv[4:])
        return 0
    if len(sys.argv) < 2:
        sys.exit(DESCRIPTION)
    return link(sys.argv[1:])


if __name__ == "__main__":
    sys.exit(main())

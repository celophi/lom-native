#!/usr/bin/env python3
from __future__ import annotations

import argparse
import bisect
from dataclasses import dataclass, field
import json
import os
from pathlib import Path
import re
import subprocess
import sys

import clang.cindex as ci

DESCRIPTION = """Write the definitions of a unit's globals, and their decoder.

    generate_unit_globals.py GAME OUT_DIR [--version us|jp] [--units A,B] [--settings unit_globals.json]

A unit is the main executable or one overlay. On the PS1 most of a unit's
globals have no C definition: the linker binds each name to its address, and
loading the unit's file fills in the bytes. Here they're ordinary C objects,
and loading the file decodes its bytes into them, turning each PS1 pointer
into a host one.

This parses the unit's sources the way the PS1 build sees them (mipsel, so
libclang gives PS1 sizes and offsets), finds every global in the unit's symbol
lists that the code declares but doesn't define, and writes one C file with:

- a definition of each one;
- a decode function for each one, which copies plain data with one memcpy and
  converts pointers and longs field by field;
- the unit's table: its globals, and its functions by PS1 address, for
  function pointers in data.

src/core/unit_decode.c is the other half. Settings, per unit, are in
tools/build/unit_globals.json.

It also writes OUT_DIR/layout.txt, how each global ended up defined, which
check_unit_layout.py compares with the repo's copy on every build.

--version picks the game: us (SLUS-01013, the default) or jp (SLPS-02170). It
chooses the symbol lists and overlay layouts under game/config/VERSION/, and
parses the code with VERSION_US or VERSION_JP. A setting that differs between
them is an object keyed by version, like "bss_end": {"us": ..., "jp": ...},
and "{version}" in a layout path is replaced.
"""

CK, TK = ci.CursorKind, ci.TypeKind

LIBCLANG_VARIABLE = "LOM_LIBCLANG"
INCLUDES_VARIABLE = "LOM_PARSE_INCLUDES"
XCODE_LIBCLANG = Path("..") / "lib" / "libclang.dylib"

TOOLS_DIR = Path(__file__).resolve().parent
DEFAULT_SETTINGS = TOOLS_DIR / "unit_globals.json"
LAYOUT_FILE = "layout.txt"
DECOMP_ASSETS = TOOLS_DIR / "decomp_assets"
PORT_INCLUDE = TOOLS_DIR.parents[1] / "include"

PARSE_FLAGS = [
    "--target=mipsel-unknown-linux-gnu",
    "-std=gnu89",
    "-DM2CTX",
    "-Wno-everything",
    "-fms-extensions",
    "-fno-builtin",
]
INCLUDE_DIRS = ("include", "include/sdk", "src")

SYMBOL = re.compile(r"^\s*([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;(.*)$")
SIZE_NOTE = re.compile(r"\bsize:(0x[0-9A-Fa-f]+|\d+)")
SCALARS = {
    TK.BOOL, TK.CHAR_U, TK.UCHAR, TK.CHAR_S, TK.SCHAR, TK.USHORT, TK.SHORT, TK.UINT, TK.INT,
    TK.ULONG, TK.LONG, TK.ULONGLONG, TK.LONGLONG, TK.FLOAT, TK.DOUBLE, TK.ENUM,
}
LONGS = {TK.ULONG, TK.LONG}
SIGNED_LONGS = {TK.LONG}


VERSIONS = ("us", "jp")

VERSION = VERSIONS[0]

SIZES: dict[str, int] = {}

UNION_RULES: dict[str, dict] = {}

DROPPED_FIELDS: set[str] = set()


def is_const(type_: ci.Type) -> bool:
    """Whether a global is const. For an array that's its element type's
    qualifier, which libclang doesn't report on the array type."""
    while type_.kind in (TK.CONSTANTARRAY, TK.INCOMPLETEARRAY):
        if type_.is_const_qualified():
            return True
        type_ = type_.element_type
    return type_.is_const_qualified()


def read_symbols(path: Path) -> dict[str, int]:
    """Names and addresses, and any size: notes into SIZES."""
    symbols = {}
    for line in path.read_text().splitlines():
        match = SYMBOL.match(line)
        if match:
            symbols[match.group(1)] = int(match.group(2), 16)
            size = SIZE_NOTE.search(match.group(3))
            if size:
                SIZES[match.group(1)] = int(size.group(1), 0)
    return symbols


def symbol_list(game: Path, list_name: str) -> Path:
    """The path of one of the game's symbol lists, for the version being built.

    @param game The game's folder.
    @param list_name The list's name, like "field".
    @return Its path.
    """
    return game / "config" / VERSION / "symbols" / f"{list_name}_symbol_addrs.txt"


def for_version(value):
    """A setting's value for the version being built.

    @param value The setting: the value itself, or an object keyed by version.
    @return The value.
    """
    return value[VERSION] if isinstance(value, dict) else value


def layout_path(game: Path, settings: dict) -> Path:
    """The unit's splat yaml, for the version being built.

    @param game The game's folder.
    @param settings The unit's settings.
    @return Its path.
    """
    return game / for_version(settings["layout"]).replace("{version}", VERSION)


def read_layout(path: Path) -> tuple[int, int]:
    """The unit's load address and image size, from its splat yaml."""
    text = path.read_text()
    vram = int(re.search(r"vram:\s*(0x[0-9A-Fa-f]+)", text).group(1), 16)
    start = int(re.search(r"^\s+start:\s*(0x[0-9A-Fa-f]+)", text, re.M).group(1), 16)
    ends = re.findall(r"^\s*- \[(0x[0-9A-Fa-f]+)\]\s*$", text, re.M)
    return vram, int(ends[-1], 16) - start


@dataclass
class Declaration:
    name: str
    type: ci.Type
    file: Path
    defined: bool
    const: bool
    includes: list[Path] = field(default_factory=list)
    spelled: bool = False
    synthesized: bool = False


@dataclass
class Unit:
    name: str
    vram: int
    image_size: int
    symbols: dict[str, int]
    shared: set[str] = field(default_factory=set)
    bss_end: int = 0
    overlay: bool = True
    runtime: set[str] = field(default_factory=set)
    element_decoders: set[str] = field(default_factory=set)
    globals: dict[str, Declaration] = field(default_factory=dict)
    functions: dict[str, Path] = field(default_factory=dict)


@dataclass
class Index:
    variables: dict[str, list[tuple[Path, "Declaration"]]] = field(default_factory=dict)
    functions: dict[str, Path] = field(default_factory=dict)


def parse_all(game: Path, sources: list[Path]) -> Index:
    """Parse every source once, as PS1 code."""
    index = Index()
    clang_index = ci.Index.create()
    for source in sources:
        args = PARSE_FLAGS + ["-I" + str(source.parent)] + ["-I" + str(game / d) for d in INCLUDE_DIRS]
        args += ["-I" + str(DECOMP_ASSETS), "-I" + str(PORT_INCLUDE)]
        tu = clang_index.parse(str(source), args=args)
        DEPENDENCIES.add(str(source))
        DEPENDENCIES.update(include.include.name for include in tu.get_includes())
        errors = [str(d) for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error]
        if errors:
            raise SystemExit(f"{source}: doesn't parse as PS1 code:\n" + "\n".join(errors[:8]))
        direct_includes = [Path(include.include.name) for include in tu.get_includes() if include.depth == 1]
        for cursor in tu.cursor.get_children():
            name = cursor.spelling
            if cursor.kind == CK.FUNCTION_DECL:
                index.functions.setdefault(name, Path(cursor.location.file.name))
            elif cursor.kind == CK.VAR_DECL:
                defined = cursor.storage_class != ci.StorageClass.EXTERN
                declaration = Declaration(name, cursor.type, Path(cursor.location.file.name), defined, is_const(cursor.type))
                if declaration.file.suffix == ".c":
                    declaration.includes = list(direct_includes)
                index.variables.setdefault(name, []).append((source, declaration))
    return index


def collect(unit: Unit, own: set[Path], compiled: set[Path], index: Index) -> None:
    """The unit's declarations: its own sources' first, then anyone's. A name
    counts as defined only if a file the native build compiles defines it."""
    for name in unit.symbols:
        if name in index.functions:
            if unit.vram <= unit.symbols[name] < unit.vram + unit.image_size:
                unit.functions[name] = index.functions[name]
            continue
        found = index.variables.get(name)
        if not found:
            continue
        if unit.overlay and name in unit.shared and not any(source in own for source, _ in found):
            continue
        ranked = sorted(found, key=lambda item: (item[0] not in own, item[1].type.kind == TK.INCOMPLETEARRAY))
        declaration = ranked[0][1]
        declaration.defined = any(d.defined and source in compiled for source, d in found)
        unit.globals[name] = declaration


def canonical(type_: ci.Type) -> ci.Type:
    """The type a typedef stands for.

    @param type_ The type.
    @return Its canonical type.
    """
    return type_.get_canonical()


POINTER_INTEGERS = {"uintptr_t", "intptr_t"}


def is_pointer_integer(type_: ci.Type) -> bool:
    """True if type_ is (a typedef of) uintptr_t or intptr_t. The canonical type
    is just an int, so this walks the typedefs."""
    while True:
        if type_.kind == TK.ELABORATED:
            type_ = type_.get_named_type()
            continue
        if type_.kind != TK.TYPEDEF:
            return False
        declaration = type_.get_declaration()
        if declaration.spelling in POINTER_INTEGERS:
            return True
        type_ = declaration.underlying_typedef_type


def is_plain(type_: ci.Type) -> bool:
    """True if the type has the same layout on the PS1 and a 64-bit host."""
    if is_pointer_integer(type_):
        return False
    type_ = canonical(type_)
    if type_.kind in LONGS:
        return False
    if type_.kind in SCALARS:
        return True
    if type_.kind == TK.POINTER:
        return False
    if type_.kind in (TK.CONSTANTARRAY, TK.INCOMPLETEARRAY):
        return is_plain(type_.element_type)
    if type_.kind == TK.RECORD:
        return not is_gpu_packet(type_) and all(is_plain(f.type) for f in type_.get_fields())
    raise SystemExit(f"don't know the layout of {type_.spelling} ({type_.kind})")


def is_gpu_packet(type_: ci.Type) -> bool:
    """A libgpu packet: a tag word first, then the color and command bytes
    (P_TAG's shape, which every primitive and drawing-mode packet starts with).
    The tag is a long named tag, or a union holding a 4-byte tag (a game type
    that also names the length byte). Natively it's PSY-Z's header (O_TAG), so
    the layout differs from the PS1's and the bytes can't be copied whole."""
    type_ = canonical(type_)
    if type_.kind != TK.RECORD:
        return False
    members = list(type_.get_fields())
    if len(members) < 2 or members[0].get_field_offsetof() != 0:
        return False
    first = canonical(members[0].type)
    if members[0].spelling == "tag" and first.kind in LONGS:
        return True
    return (first.kind == TK.RECORD and first.get_declaration().kind == CK.UNION_DECL
            and any(m.spelling == "tag" and m.type.get_size() == 4 for m in first.get_fields())
            and any(m.spelling == "code" and m.get_field_offsetof() == 56 for m in members))


def emit_pointer(out: list[str], pad: str, dst: str, type_: ci.Type, src: str, where: str, encode: bool) -> None:
    """Convert a pointer: to data, through the unit's globals, or to code, through its functions."""
    pointee = canonical(type_.get_pointee())
    code = pointee.kind in (TK.FUNCTIONPROTO, TK.FUNCTIONNOPROTO)
    if encode:
        finder = "lom_unit_ps1_code_address" if code else "lom_unit_ps1_pointer"
        out.append(f"{pad}lom_unit_write_u32({src}, {finder}((const void*){dst}, \"{where}\"));")
    elif code:
        out.append(f"{pad}{dst} = (__typeof__({dst}))lom_unit_decode_code(context, lom_unit_read_u32({src}), \"{where}\");")
    else:
        out.append(f"{pad}{dst} = (__typeof__({dst}))lom_unit_decode_pointer(context, lom_unit_read_u32({src}), \"{where}\");")


def emit_small_or_pointer(out: list[str], pad: str, dst: str, type_: ci.Type, src: str, where: str, encode: bool) -> None:
    """Convert a union of a small number or a pointer: a word below its split is the number."""
    rule = union_rule(type_, where)
    pointer_member = next(m for m in type_.get_fields() if m.spelling == rule["pointer"])
    code = canonical(canonical(pointer_member.type).get_pointee()).kind in (TK.FUNCTIONPROTO, TK.FUNCTIONNOPROTO)
    small, pointer, split = f"{dst}.{rule['small']}", f"{dst}.{rule['pointer']}", rule["split"]
    if encode:
        finder = "lom_unit_ps1_code_address" if code else "lom_unit_ps1_pointer"
        out.append(f"{pad}if ((__UINTPTR_TYPE__){pointer} < {split}u)")
        out.append(f"{pad}    lom_unit_write_u32({src}, (unsigned int){small});")
        out.append(f"{pad}else")
        out.append(f"{pad}    lom_unit_write_u32({src}, {finder}((const void*){pointer}, \"{where}\"));")
    else:
        decoder = "lom_unit_decode_code" if code else "lom_unit_decode_pointer"
        out.append(f"{pad}memset((void*)&{dst}, 0, sizeof({dst}));")
        out.append(f"{pad}if (lom_unit_read_u32({src}) < {split}u)")
        out.append(f"{pad}    {small} = lom_unit_read_u32({src});")
        out.append(f"{pad}else")
        out.append(f"{pad}    {pointer} = (__typeof__({pointer})){decoder}(context, lom_unit_read_u32({src}), \"{where}\");")


def emit_packet_tag(out: list[str], pad: str, dst: str, src: str, where: str, encode: bool) -> None:
    """Convert a GPU packet's tag: its length, and its link to the next packet."""
    if encode:
        out.append(f"{pad}lom_unit_write_u32({src}, (unsigned int)getlen(&{dst}) << 24 | (getaddr(&{dst}) == 0xFFFFFF ? 0xFFFFFFu : "
                   f"lom_unit_ps1_pointer((const void*)getaddr(&{dst}), \"{where}.tag\") & 0xFFFFFFu));")
    else:
        out.append(f"{pad}{{")
        out.append(f"{pad}    unsigned int word = lom_unit_read_u32({src});")
        out.append(f"{pad}    unsigned int link = word & 0xFFFFFFu;")
        out.append(f"{pad}    setlen(&{dst}, word >> 24);")
        out.append(f"{pad}    setaddr(&{dst}, link == 0xFFFFFFu || link == 0 ? link : "
                   f"(__UINTPTR_TYPE__)lom_unit_decode_pointer(context, 0x80000000u | link, \"{where}.tag\"));")
        out.append(f"{pad}}}")


def emit_record(out: list[str], pad: str, dst: str, type_: ci.Type, src: str, depth: int, where: str, encode: bool) -> None:
    """Convert a struct member by member, bit-fields included, with a GPU packet's tag first."""
    members = list(type_.get_fields())
    if is_gpu_packet(type_):
        emit_packet_tag(out, pad, dst, src, where, encode)
        members = members[1:]
    type_name = canonical(type_).spelling.replace("struct ", "").replace("union ", "")
    members = [m for m in members if f"{type_name}.{m.spelling}" not in DROPPED_FIELDS]
    for member in members:
        if not member.spelling:
            raise SystemExit(f"{where}: an anonymous member in {type_.spelling}")
        if member.is_bitfield():
            bits, width = member.get_field_offsetof(), member.get_bitfield_width()
            if encode:
                out.append(f"{pad}lom_unit_write_bits({src}, {bits}, {width}, (unsigned long long){dst}.{member.spelling});")
            else:
                out.append(f"{pad}{dst}.{member.spelling} = lom_unit_read_bits({src}, {bits}, {width});")
            continue
        offset = member.get_field_offsetof() // 8
        emit_decode(out, f"{dst}.{member.spelling}", member.type, f"{src} + {offset}", depth, f"{where}.{member.spelling}", encode)


def emit_decode(out: list[str], dst: str, type_: ci.Type, src: str, depth: int, where: str, encode: bool = False) -> None:
    """C statements that decode one PS1 value at src into the lvalue dst, or, with
    encode, write dst back to PS1 bytes at src."""
    pad = "    " * (depth + 1)
    if is_pointer_integer(type_):
        if encode:
            out.append(f"{pad}lom_unit_write_u32({src}, lom_unit_ps1_pointer((const void*){dst}, \"{where}\"));")
        else:
            out.append(f"{pad}{dst} = (__typeof__({dst}))lom_unit_decode_pointer(context, lom_unit_read_u32({src}), \"{where}\");")
        return
    type_ = canonical(type_)
    size = type_.get_size()
    union = type_.kind == TK.RECORD and type_.get_declaration().kind == CK.UNION_DECL
    if is_plain(type_):
        out.append(f"{pad}_Static_assert(sizeof({dst}) == {size}, \"{where}: native layout differs\");")
        out.append(f"{pad}memcpy({src}, (const void*)&{dst}, {size});" if encode else f"{pad}memcpy((void*)&{dst}, {src}, {size});")
    elif type_.kind in LONGS:
        if encode:
            out.append(f"{pad}lom_unit_write_u32({src}, (unsigned int){dst});")
        else:
            reader = "lom_unit_read_s32" if type_.kind in SIGNED_LONGS else "lom_unit_read_u32"
            out.append(f"{pad}{dst} = {reader}({src});")
    elif type_.kind == TK.POINTER:
        emit_pointer(out, pad, dst, type_, src, where, encode)
    elif type_.kind == TK.CONSTANTARRAY:
        element = type_.element_type
        stride = canonical(element).get_size()
        index = "ijklmn"[depth]
        out.append(f"{pad}for (int {index} = 0; {index} < {type_.element_count}; {index}++)")
        out.append(f"{pad}{{")
        emit_decode(out, f"{dst}[{index}]", element, f"{src} + {index} * {stride}", depth + 1, where, encode)
        out.append(f"{pad}}}")
    elif union and all_data_pointers(type_):
        first = next(iter(type_.get_fields()))
        emit_decode(out, f"{dst}.{first.spelling}", first.type, src, depth, f"{where}.{first.spelling}", encode)
    elif union:
        emit_small_or_pointer(out, pad, dst, type_, src, where, encode)
    elif type_.kind == TK.RECORD:
        emit_record(out, pad, dst, type_, src, depth, where, encode)
    else:
        raise SystemExit(f"{where}: can't decode {type_.spelling}")


def include_name(game: Path, path: Path) -> str:
    """Spell a header the way the game's code includes it.

    @param game The game's folder.
    @param path The header.
    @return Its path from include/, src/ or the decomp's assets.
    """
    for root in (game / "include", game / "src", DECOMP_ASSETS):
        try:
            return path.relative_to(root).as_posix()
        except ValueError:
            pass
    raise SystemExit(f"{path}: a declaration outside include/ and src/")


def defined_in_source_file(type_: ci.Type) -> bool:
    """True if the type, or a typedef or record it's built from, is declared in a
    .c file, where nothing else can name it."""
    while True:
        declaration = type_.get_declaration()
        if declaration.location.file is not None and declaration.location.file.name.endswith(".c"):
            return True
        if type_.kind == TK.TYPEDEF:
            type_ = declaration.underlying_typedef_type
        elif type_.kind in (TK.CONSTANTARRAY, TK.INCOMPLETEARRAY):
            type_ = type_.element_type
        elif type_.kind == TK.POINTER:
            type_ = type_.get_pointee()
        elif type_.kind == TK.ELABORATED:
            type_ = type_.get_named_type()
        else:
            return False


def all_data_pointers(type_: ci.Type) -> bool:
    """Check every member of a struct is a pointer to data.

    @param type_ The struct.
    @return Whether they all are.
    """
    members = [canonical(m.type) for m in type_.get_fields()]
    return bool(members) and all(
        m.kind == TK.POINTER and canonical(m.get_pointee()).kind not in (TK.FUNCTIONPROTO, TK.FUNCTIONNOPROTO) for m in members
    )


def union_rule(type_: ci.Type, where: str) -> dict:
    """Find how to encode a union that has pointers or longs in it.

    @param type_ The union.
    @param where Where it's used, for the error.
    @return Its rule from the settings.
    """
    name = type_.get_declaration().spelling or type_.spelling
    for key, rule in UNION_RULES.items():
        if key == name or key == type_.spelling or type_.spelling.endswith(key):
            return rule
    raise SystemExit(f"{where}: a union with pointers or longs in it ({type_.spelling}); give it a rule in unions")


def synthesize(type_: ci.Type, where: str) -> str:
    """A native type with the same layout as type_, built from its PS1 shape, for
    a global whose type only its own .c file can name. Integers keep their size,
    longs stay longs, every pointer is a void*, and records keep their member
    names, so the same field paths decode it."""
    if is_pointer_integer(type_):
        return "unsigned long"
    type_ = canonical(type_)
    if type_.kind in LONGS:
        return "long" if type_.kind in SIGNED_LONGS else "unsigned long"
    if type_.kind in (TK.FLOAT, TK.DOUBLE):
        return type_.spelling
    if type_.kind in SCALARS:
        return {1: "unsigned char", 2: "unsigned short", 4: "unsigned int", 8: "unsigned long long"}[type_.get_size()]
    if type_.kind == TK.POINTER or is_pointer_integer(type_):
        return "void*"
    if type_.kind == TK.CONSTANTARRAY:
        return f"__typeof__(__typeof__({synthesize(type_.element_type, where)})[{type_.element_count}])"
    if type_.kind == TK.RECORD and is_plain(type_):
        size, align = type_.get_size(), type_.get_align()
        if size in (1, 2, 4, 8) and align == size:
            return {1: "unsigned char", 2: "unsigned short", 4: "unsigned int", 8: "unsigned long long"}[size]
        return f"struct __attribute__((aligned({align}))) {{ unsigned char bytes[{size}]; }}"
    if type_.kind == TK.RECORD and type_.get_declaration().kind == CK.UNION_DECL and all_data_pointers(type_):
        return "union { " + " ".join(f"void* {m.spelling};" for m in type_.get_fields()) + " }"
    if type_.kind == TK.RECORD and type_.get_declaration().kind == CK.UNION_DECL:
        rule = union_rule(type_, where)
        members = {m.spelling: m for m in type_.get_fields()}
        return (f"union {{ __typeof__({synthesize(members[rule['small']].type, where)}) {rule['small']}; "
                f"void* {rule['pointer']}; }}")
    if type_.kind == TK.RECORD and type_.get_declaration().kind == CK.STRUCT_DECL:
        members = []
        fields = list(type_.get_fields())
        if is_gpu_packet(type_):
            members.append("u_long tag; u_long len;")
            fields = fields[1:]
        for member in fields:
            if member.is_bitfield() or not member.spelling:
                raise SystemExit(f"{where}: can't rebuild {type_.spelling} natively (a bitfield or an anonymous member)")
            members.append(f"__typeof__({synthesize(member.type, where)}) {member.spelling};")
        return "struct { " + " ".join(members) + " }"
    raise SystemExit(f"{where}: can't rebuild {type_.spelling} natively")


def _includable(game: Path, path: Path) -> bool:
    """Check a header can be included from a unit's globals file.

    @param game The game's folder.
    @param path The header.
    @return Whether it can.
    """
    roots = [game / "include", game / "src", PORT_INCLUDE, DECOMP_ASSETS]
    return any(path.is_relative_to(root) for root in roots) and path.suffix == ".h"


def include_line(game: Path, path: Path) -> str:
    """How the generated file includes a header. SDK headers go by their plain
    name, so the native build's own choice of SDK header applies."""
    if path.is_relative_to(game / "include/sdk"):
        return f"#include <{path.name}>"
    if path.is_relative_to(PORT_INCLUDE):
        return f'#include "{path.relative_to(PORT_INCLUDE).as_posix()}"'
    return f'#include "{include_name(game, path)}"'


def find_aliases(unit: Unit, globals_: list[dict], problems: list[str]) -> list[dict]:
    """A global inside another one is a second name for some of its bytes (say,
    the high half of a word). Natively it has to stay the same bytes, so it
    becomes an alias at the same offset. Only a plain container can have one,
    since offsets inside it are the same on both."""
    kept: list[dict] = []
    for g in sorted(globals_, key=lambda g: (g["address"], -g["size"])):
        outer = next((k for k in reversed(kept) if k["address"] <= g["address"] < k["address"] + k["size"]), None)
        if outer is None:
            kept.append(g)
            continue
        offset = g["address"] - outer["address"]
        plain = outer.get("container") or is_plain(canonical(outer["declaration"].type))
        if not plain or g["address"] + g["size"] > outer["address"] + outer["size"] or g.get("separate"):
            g["overlaps"] = f"{offset} bytes into {outer['name']}"
            kept.append(g)
            continue
        g["alias_of"], g["offset"] = outer["name"], offset
        kept.append(g)
    return kept


def spelled_out_here(declaration) -> bool:
    """Say whether a global's type has to be written out in the globals file: it has no name to use, or it's only
    defined in a source file the globals file can't include.

    @param declaration The global's declaration.
    @return Whether it does.
    """
    spelling = declaration.type.spelling
    return ("(unnamed" in spelling or "(anonymous" in spelling or declaration.file.name.endswith(".inc.c")
            or defined_in_source_file(declaration.type))


def sized_entry(name: str, address: int, type_, declaration, span: int, problems: list[str]) -> dict | None:
    """A global's entry with its size. An array declared without one runs to the next symbol, or to its size in
    SIZES, and has to be a whole number of elements.

    @param name The global.
    @param address Its PS1 address.
    @param type_ Its canonical type.
    @param declaration Its declaration.
    @param span The bytes from it to the next symbol.
    @param problems Where to add why it can't be sized.
    @return The entry, or None when it can't be sized.
    """
    if type_.kind != TK.INCOMPLETEARRAY:
        return dict(name=name, address=address, size=type_.get_size(), declaration=declaration)
    stride = canonical(type_.element_type).get_size()
    span = SIZES.get(name, span)
    if span % stride:
        problems.append(f"{name}: {span} bytes to the next symbol isn't a whole number of elements; give it a size")
        return None
    return dict(name=name, address=address, size=span, count=span // stride, declaration=declaration)


def plan(game: Path, unit: Unit, settings: dict) -> list[dict]:
    """Every global this unit defines natively, with its PS1 address and size."""
    containers = settings.get("containers", {})
    skip = set(settings.get("skip", []))
    addresses = sorted(set(address for name, address in unit.symbols.items()
                           if not (unit.overlay and name in unit.shared and name in unit.functions)))
    image_end = unit.bss_end
    file_end = unit.vram + unit.image_size

    def next_symbol(address: int) -> int:
        """Where the data at address ends at the latest: the next symbol, or the
        end of the file for data in it."""
        later = addresses[bisect.bisect_right(addresses, address):][:1]
        if address < file_end:
            later.append(file_end)
        return min(later) if later else image_end

    globals_ = []
    problems = []
    for name, declaration in sorted(unit.globals.items(), key=lambda item: unit.symbols[item[0]]):
        address = unit.symbols[name]
        if declaration.defined or name in skip or not (unit.vram <= address < image_end):
            continue
        if declaration.file.is_relative_to(game / "include/sdk"):
            continue
        type_ = canonical(declaration.type)
        if name in containers:
            end = unit.symbols[containers[name]]
            globals_.append(dict(name=name, address=address, size=end - address, container=True, declaration=declaration))
            continue
        span = next_symbol(address) - address
        if declaration.file.suffix != ".h":
            if is_plain(type_):
                size = SIZES.get(name, span) if type_.kind == TK.INCOMPLETEARRAY else type_.get_size()
                globals_.append(dict(name=name, address=address, size=size, container=True, declaration=declaration))
                continue
            if spelled_out_here(declaration):
                declaration.synthesized = True
                declaration.includes = []
            else:
                declaration.includes = [path for path in declaration.includes if path.suffix == ".h"]
                outside = [path.name for path in declaration.includes if not _includable(game, path)]
                if outside:
                    problems.append(f"{name} is declared in {declaration.file.name}, which includes headers from outside the game: {', '.join(outside)}")
                    continue
                declaration.spelled = True
        entry = sized_entry(name, address, type_, declaration, span, problems)
        if entry:
            globals_.append(entry)
    for g in globals_:
        g["separate"] = g["name"] in settings.get("separate", [])
    globals_ = find_aliases(unit, globals_, problems)
    if problems:
        raise SystemExit(f"{unit.name}:\n  " + "\n  ".join(problems))
    return globals_


def shared_globals_are_weak(unit: Unit) -> bool:
    """Say whether a unit's shared globals are weak, which they are for overlays.

    @param unit The unit.
    @return Whether they are.
    """
    return unit.overlay


def header_lines(game: Path, globals_: list[dict]) -> list[str]:
    """The includes a unit's globals file needs: the headers that declare its globals.

    @param game The game's folder.
    @param globals_ The unit's globals.
    @return One include line per header, in the order they're first needed.
    """
    headers: list[str] = []
    for g in globals_:
        declaration = g["declaration"]
        paths = declaration.includes if declaration.spelled else [declaration.file] if declaration.file.suffix == ".h" else []
        for path in paths:
            line = include_line(game, path)
            if line not in headers:
                headers.append(line)
    return headers


def storage(name: str) -> str:
    """The name of the storage behind a global the code can't define under its own name.

    @param name The global.
    @return Its storage's name, bound to the global's symbol.
    """
    return f"lom_unit_storage_{name}"


def define(lines: list[str], g: dict) -> str:
    """Write a global's definition.

    A global defined under its own name is the type the code declares. A
    container, a type only one source file knows, or a const global gets
    storage under another name, bound to the global's symbol.

    @param lines The file's lines, which this adds to.
    @param g The global.
    @return The name the definition has, which the decoder writes to.
    """
    name, declaration = g["name"], g["declaration"]
    type_ = canonical(declaration.type)
    bound = f"__asm__(LOM_UNIT_SYMBOL(\"{name}\"))"
    if g.get("container"):
        lines.append(f"_Alignas(16) unsigned char {storage(name)}[{g['size']}] {bound};")
        return storage(name)
    if declaration.synthesized:
        if type_.kind == TK.INCOMPLETEARRAY:
            rebuilt = f"__typeof__({synthesize(type_.element_type, name)})[{g['count']}]"
        else:
            rebuilt = synthesize(type_, name)
        lines.append(f"/* {name}'s type is {declaration.file.name}'s own; this has its layout. */")
        lines.append(f"__typeof__({rebuilt}) {storage(name)} {bound};")
        return storage(name)
    if declaration.spelled:
        spelling = declaration.type.spelling
        if declaration.const:
            lines.append(f"__typeof_unqual__({spelling}) {storage(name)} {bound};")
            return storage(name)
        if type_.kind == TK.INCOMPLETEARRAY:
            lines.append(f"__typeof__({declaration.type.element_type.spelling}) {name}[{g['count']}];")
        else:
            lines.append(f"__typeof__({spelling}) {name};")
        return name
    if type_.kind == TK.INCOMPLETEARRAY:
        if declaration.const:
            lines.append(f"__typeof_unqual__({name}[0]) {storage(name)}[{g['count']}] {bound};")
            return storage(name)
        lines.append(f"__typeof__({name}[0]) {name}[{g['count']}];")
        return name
    if declaration.const:
        lines.append(f"__typeof_unqual__({name}) {storage(name)} {bound};")
        return storage(name)
    lines.append(f"__typeof__({name}) {name};")
    return name


def convert(lines: list[str], g: dict, encode: bool) -> None:
    """Write the body that turns a global's PS1 bytes into it, or it back into them.

    @param lines The file's lines, which this adds to.
    @param g The global, with its target and whether it's plain.
    @param encode True to write the global into PS1 bytes, False to read them.
    """
    name, target = g["name"], g["target"]
    type_ = canonical(g["declaration"].type)
    if g["plain"]:
        if encode:
            lines.append(f"    memcpy(source, (const void*)&{target}, {g['size']});")
        else:
            lines.append(f"    _Static_assert(sizeof({target}) == {g['size']}, \"{name}: native layout differs\");")
            lines.append(f"    memcpy((void*)&{target}, source, {g['size']});")
    elif type_.kind == TK.INCOMPLETEARRAY:
        element = type_.element_type
        stride = canonical(element).get_size()
        lines.append(f"    for (int i = 0; i < {g['count']}; i++)")
        lines.append("    {")
        emit_decode(lines, f"{target}[i]", element, f"source + i * {stride}", 1, name, encode=encode)
        lines.append("    }")
    else:
        emit_decode(lines, target, type_, "source", 0, name, encode=encode)


def write_runtime_converters(lines: list[str], g: dict) -> None:
    """Write the decoder and encoder of a global the code builds itself: its
    bytes in the file must be zero, and it starts zeroed.

    @param lines The file's lines, which this adds to.
    @param g The global.
    """
    name, target = g["name"], g["target"]
    lines.append(f"static void decode_{name}(const unsigned char* source, LomUnitDecodeContext* context)")
    lines.append("{")
    lines.append("    (void)context;")
    lines.append(f"    lom_unit_expect_zero(source, {g['size']}u, \"{name}\");")
    lines.append(f"    memset((void*)&{target}, 0, sizeof({target}));")
    lines.append("}")
    lines.append(f"static void encode_{name}(unsigned char* source)")
    lines.append("{")
    lines.append(f"    memset(source, 0, {g['size']}u);")
    lines.append("}")


def write_converters(lines: list[str], g: dict) -> None:
    """Write a global's decoder and encoder.

    @param lines The file's lines, which this adds to.
    @param g The global.
    """
    name = g["name"]
    if "overlaps" in g:
        lines.append(f"/* Overlaps: {name} starts {g['overlaps']} on the PS1. It's a separate object here. */")
    lines.append(f"static void decode_{name}(const unsigned char* source, LomUnitDecodeContext* context)")
    lines.append("{")
    lines.append("    (void)context;")
    convert(lines, g, encode=False)
    lines.append("}")
    lines.append(f"static void encode_{name}(unsigned char* source)")
    lines.append("{")
    convert(lines, g, encode=True)
    lines.append("}")


def write_element_converters(lines: list[str], g: dict) -> None:
    """Write the one-element decoder and encoder of an array global, for the
    native build's views of what the PS1 reads past its end.

    @param lines The file's lines, which this adds to.
    @param g The global, an array.
    """
    name, target = g["name"], g["target"]
    type_ = canonical(g["declaration"].type)
    if type_.kind not in (TK.CONSTANTARRAY, TK.INCOMPLETEARRAY) or g.get("container"):
        raise SystemExit(f"{name}: element_decoders needs an array global")
    element = type_.element_type
    lines.append(f"void lom_unit_decode_element_{name}(const unsigned char* source, void* destination);")
    lines.append(f"void lom_unit_decode_element_{name}(const unsigned char* source, void* destination)")
    lines.append("{")
    lines.append("    LomUnitDecodeContext* context = NULL;")
    lines.append(f"    __typeof__(&{target}[0]) element = destination;")
    lines.append("    (void)context;")
    if g["plain"]:
        lines.append("    memcpy((void*)element, source, sizeof(*element));")
    else:
        emit_decode(lines, "(*element)", element, "source", 0, name)
    lines.append("}")
    lines.append(f"void lom_unit_encode_element_{name}(const void* element_source, unsigned char* source);")
    lines.append(f"void lom_unit_encode_element_{name}(const void* element_source, unsigned char* source)")
    lines.append("{")
    lines.append(f"    const __typeof__({target}[0])* element = element_source;")
    if g["plain"]:
        lines.append("    memcpy(source, (const void*)element, sizeof(*element));")
    else:
        emit_decode(lines, "(*element)", element, "source", 0, name, encode=True)
    lines.append("}")


def write_global(lines: list[str], unit: Unit, g: dict) -> None:
    """Write a global's definition and, for one in the unit's file, its converters.

    @param lines The file's lines, which this adds to.
    @param unit The unit.
    @param g The global; this notes its target, whether it's plain, its converters and its layout line.
    """
    name = g["name"]
    start = len(lines)
    if name in unit.shared and shared_globals_are_weak(unit):
        lines.append("__attribute__((weak))")
    g["plain"] = g.get("container") or is_plain(canonical(g["declaration"].type))
    g["target"] = define(lines, g)
    g["layout"] = f"0x{g['address']:08X} {g['size']} " + " ".join(lines[start:])
    if g["address"] >= unit.vram + unit.image_size:
        g["decoder"] = g["encoder"] = "NULL"
        lines.append("")
        return
    g["decoder"], g["encoder"] = f"decode_{name}", f"encode_{name}"
    if name in unit.runtime:
        write_runtime_converters(lines, g)
        lines.append("")
        return
    write_converters(lines, g)
    if name in unit.element_decoders:
        write_element_converters(lines, g)
    lines.append("")


def write_aliases(lines: list[str], unit: Unit, aliases: list[dict]) -> None:
    """Bind each alias's symbol to its offset inside the global that holds it.

    @param lines The file's lines, which this adds to.
    @param unit The unit.
    @param aliases The globals that are part of another one.
    """
    for g in aliases:
        symbol = f"LOM_UNIT_SYMBOL(\"{g['name']}\")"
        if g["name"] in unit.shared and shared_globals_are_weak(unit):
            binding = f"LOM_UNIT_WEAK({symbol})"
        else:
            binding = f"\".globl \" {symbol}"
        lines.append(f"/* {g['name']} is {g['offset']} bytes into {g['alias_of']}, as on the PS1. */")
        lines.append(
            f"__asm__({binding} \"\\n\\t.set \" {symbol} "
            f"\", \" LOM_UNIT_SYMBOL(\"{g['alias_of']}\") \" + {g['offset']}\");"
        )
    if aliases:
        lines.append("")


def write_tables(lines: list[str], unit: Unit, globals_: list[dict]) -> None:
    """Write the unit's table: its globals with their converters, and its
    functions by PS1 address.

    @param lines The file's lines, which this adds to.
    @param unit The unit.
    @param globals_ Its globals, without the aliases.
    """
    lines.append("static const LomUnitGlobal globals[] = {")
    for g in globals_:
        type_ = canonical(g["declaration"].type)
        stride_ps1 = stride_host = "0"
        if not g["plain"] and type_.kind in (TK.CONSTANTARRAY, TK.INCOMPLETEARRAY):
            stride_ps1 = str(canonical(type_.element_type).get_size())
            stride_host = f"sizeof({g['target']}[0])"
        lines.append(
            f"    {{\"{g['name']}\", 0x{g['address']:08X}u, {g['size']}u, (void*)&{g['target']}, sizeof({g['target']}), "
            f"{'1' if g['plain'] else '0'}, {stride_ps1}u, {stride_host}, {g['decoder']}, {g['encoder']}}},"
        )
    lines.append("};")
    lines.append("")

    functions = sorted((unit.symbols[name], name) for name in unit.functions)
    for _address, name in functions:
        lines.append(f"extern const char lom_unit_code_{name}[] __asm__(LOM_UNIT_SYMBOL(\"{name}\")) __attribute__((weak));")
    lines.append("")
    lines.append("static const LomUnitFunction functions[] = {")
    for address, name in functions:
        lines.append(f"    {{0x{address:08X}u, lom_unit_code_{name}, \"{name}\"}},")
    if not functions:
        lines.append("    {0, NULL, NULL},")
    lines.append("};")
    lines.append("")
    lines.append(f"const LomUnit lom_unit_{unit.name} = {{")
    lines.append(f"    \"{unit.name}\", 0x{unit.vram:08X}u, 0x{unit.image_size:X}u,")
    lines.append("    globals, sizeof(globals) / sizeof(globals[0]),")
    lines.append(f"    functions, {len(functions)},")
    lines.append("};")


def layout_lines(unit: Unit, globals_: list[dict]) -> list[str]:
    """What decides where a unit's globals sit in memory, for check_unit_layout.py.

    @param unit The unit, after write() has defined its globals.
    @param globals_ Its globals.
    @return A heading, then one line per global in address order: its PS1 address and size, then its
            definition, or for an alias, where it is inside the global that holds it.
    """
    out = [f"[{unit.name}]"]
    for g in sorted(globals_, key=lambda g: (g["address"], -g["size"])):
        if "alias_of" in g:
            out.append(f"0x{g['address']:08X} {g['size']} {g['name']} is {g['offset']} bytes into {g['alias_of']}")
        else:
            out.append(g["layout"])
    return out


def write(game: Path, unit: Unit, globals_: list[dict], out: Path) -> None:
    """Write a unit's globals file: each global defined at its PS1 address, with its PS1 bytes.

    @param game The game's folder.
    @param unit The unit.
    @param globals_ Its globals.
    @param out The file.
    """
    lines = [
        f"/* Generated by tools/build/generate_unit_globals.py for {unit.name}. Don't edit. */",
        "#include <string.h>",
        '#include "lom_native_unit_decode.h"',
        '#include "common.h"',
        *header_lines(game, globals_),
        "",
    ]
    aliases = [g for g in globals_ if "alias_of" in g]
    owned = [g for g in globals_ if "alias_of" not in g]
    for g in owned:
        write_global(lines, unit, g)
    write_aliases(lines, unit, aliases)
    write_tables(lines, unit, owned)
    out.write_text("\n".join(lines) + "\n")


def unit_sources(game: Path, settings: dict) -> list[Path]:
    """The unit's .c files: its source patterns, less its excludes. .inc.c files
    are parts of others, not units of their own."""
    excluded = {game / path for path in settings.get("exclude", [])}
    sources: list[Path] = []
    for pattern in settings["sources"]:
        sources += sorted(path for path in game.glob(pattern) if not path.name.endswith(".inc.c") and path not in excluded)
    return sources


DEPENDENCIES: set[str] = set()


def load_unit(game: Path, name: str, settings: dict) -> Unit:
    """Read a unit's settings, symbols and sources.

    @param game The game's folder.
    @param name The unit's name.
    @param settings Its settings.
    @return The unit.
    """
    symbols: dict[str, int] = {}
    shared: set[str] = set()
    for list_name in settings["symbols"]:
        listed = read_symbols(symbol_list(game, list_name))
        symbols.update(listed)
        if list_name == "shared":
            shared |= set(listed)
    for list_name in settings["symbols"]:
        if list_name != "shared":
            shared -= set(read_symbols(symbol_list(game, list_name)))
    if "layout" in settings:
        vram, image_size = read_layout(layout_path(game, settings))
    else:
        vram, image_size = int(for_version(settings["vram"]), 16), int(for_version(settings["image_size"]), 16)
    unit = Unit(name, vram, image_size, symbols, shared=shared)
    unit.bss_end = int(for_version(settings["bss_end"]), 16) if "bss_end" in settings else vram + image_size
    unit.overlay = settings.get("overlay", True)
    unit.runtime = set(settings.get("runtime", []))
    unit.element_decoders = set(settings.get("element_decoders", []))
    if not unit.overlay:
        for path in sorted((game / "config" / VERSION / "symbols").glob("*_symbol_addrs.txt")):
            for symbol, address in read_symbols(path).items():
                if unit.vram <= address < unit.bss_end:
                    unit.symbols.setdefault(symbol, address)
    for guest in settings.get("hosts", []):
        for symbol, address in read_symbols(symbol_list(game, guest)).items():
            if unit.vram <= address < unit.vram + unit.image_size:
                unit.symbols.setdefault(symbol, address)
    return unit


def choose_libclang() -> None:
    """Point the bindings at LOM_LIBCLANG if it's set, or at Xcode's libclang on macOS.

    Anywhere else the bindings keep the library they find themselves.
    """
    library = os.environ.get(LIBCLANG_VARIABLE)
    if not library and sys.platform == "darwin":
        clang = subprocess.run(["xcrun", "--find", "clang"], capture_output=True, text=True, check=True).stdout.strip()
        library = str((Path(clang).parent / XCODE_LIBCLANG).resolve())
    if library:
        ci.Config.set_library_file(library)
    if sys.platform == "win32":
        resource_dir = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True, check=True).stdout.strip()
        PARSE_FLAGS.extend(["-resource-dir", resource_dir])


def extra_includes() -> list[str]:
    """The folders in LOM_PARSE_INCLUDES, as -idirafter flags."""
    folders = os.environ.get(INCLUDES_VARIABLE, "").split(os.pathsep)
    return [flag for folder in folders if folder for flag in ("-idirafter", folder)]


def main() -> int:
    """Write each unit's globals file, or list a unit's sources.

    @return 0.
    """
    choose_libclang()
    PARSE_FLAGS.extend(extra_includes())
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("game", type=Path, help="the game's source folder (game/)")
    parser.add_argument("out_dir", type=Path, help="where to write UNIT_globals.c for each unit, and layout.txt for check_unit_layout.py")
    parser.add_argument("--version", choices=VERSIONS, default=VERSIONS[0], help="the game to build: us (the default) or jp")
    parser.add_argument("--units", help="the units to write, separated by commas (all of them by default)")
    parser.add_argument("--settings", type=Path, default=DEFAULT_SETTINGS)
    parser.add_argument("--depfile", type=Path, help="write a Makefile depfile of everything parsed")
    parser.add_argument("--list-sources", metavar="UNIT", help="print UNIT's sources, separated by ;, and stop")
    args = parser.parse_args()
    global VERSION
    VERSION = args.version
    PARSE_FLAGS.append(f"-DVERSION_{VERSION.upper()}")

    game = args.game.resolve()
    all_settings = json.loads(args.settings.read_text())["units"]
    if args.list_sources:
        print(";".join(str(path) for path in unit_sources(game, all_settings[args.list_sources])))
        return 0

    UNION_RULES.update(json.loads(args.settings.read_text()).get("unions", {}))
    DROPPED_FIELDS.update(json.loads(args.settings.read_text()).get("dropped_fields", []))

    compiled: set[Path] = set()
    everything: set[Path] = set()
    own: dict[str, set[Path]] = {}
    for name, settings in all_settings.items():
        own[name] = set(unit_sources(game, settings))
        compiled |= own[name]
        everything |= own[name] | set(unit_sources(game, {**settings, "exclude": []}))
    index = parse_all(game, sorted(everything))

    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / "data_names.txt").write_text("\n".join(sorted(index.variables)) + "\n")

    wanted = args.units.split(",") if args.units else list(all_settings)
    outputs = []
    layout: list[str] = []
    for name in wanted:
        settings = all_settings[name]
        unit = load_unit(game, name, settings)
        collect(unit, own[name], compiled, index)
        globals_ = plan(game, unit, settings)
        out = args.out_dir / f"{name}_globals.c"
        out.parent.mkdir(parents=True, exist_ok=True)
        write(game, unit, globals_, out)
        layout.extend(layout_lines(unit, globals_))
        outputs.append(out)
        DEPENDENCIES.update(str(symbol_list(game, list_name)) for list_name in settings["symbols"])
        if "layout" in settings:
            DEPENDENCIES.add(str(layout_path(game, settings)))
        print(f"{name}: {len(globals_)} globals, {len(unit.functions)} functions")
    (args.out_dir / LAYOUT_FILE).write_text("\n".join(layout) + "\n")
    if args.depfile:
        names = " ".join(sorted(path.replace(" ", "\\ ") for path in DEPENDENCIES))
        args.depfile.write_text(" ".join(str(out) for out in outputs) + f": {names}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())

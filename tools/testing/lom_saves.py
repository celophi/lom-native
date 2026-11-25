#!/usr/bin/env python3
from __future__ import annotations

import argparse
import base64
import functools
import json
import re
import struct
import subprocess
import sys
import threading
from pathlib import Path

DESCRIPTION = """Build Legend of Mana saves for tests, and read saves back.

The route tests start from saves this tool writes, and their checkpoints read
the saves the game writes. It doesn't use the game's code. It starts from the
new-game state on your disc (the template TITLE copies when you pick New Game),
applies a save description, and writes a memory-card file with a valid
checksum. Field offsets come from lom-save-layout, which is compiled against
the decomp's own save types, so they can't drift from the game's.

    lom_saves.py build <description.json> --disc <disc> --layout <lom-save-layout> --card-root <folder>
    lom_saves.py read <save file> --layout <lom-save-layout>

A save description is JSON:

    {
      "format": 1,
      "description": "A new game, saved at the bedroom desk",
      "hero": 0,
      "scene": {"scene": 19, "object": 0, "music": 9, "sound_bank": 3, "secondary_music": 255, "track": 0},
      "money": 100,
      "variables": {"0x2310": 12},
      "words": {"24": "0xf2f20000"},
      "item_counts": {"0x40": 3}
    }

Only "format" and "description" are required. "variables" uses the scripts'
own variable references (the ones the scene disassembly prints), so a route
can set story progress the same way the game's scripts do. "words" sets whole
words of the saved game's script-variable words by index, for a state copied
from a real game (like the one after the opening). "item_counts" sets how many
of an item kind (materials, coins, produce: the kinds held as counts) the party
holds. "encyclopedia": "all" unlocks every encyclopedia entry. "logic_blocks" gives the
golem workshop logic blocks: [id, level, shape] unassigned, or [id, level, shape, logic type,
rotation, x, y] placed on that logic type's grid. "hatched_pets" marks those stored pet records
(by index) as just hatched.
"""

REPO = Path(__file__).resolve().parents[2]
DECOMP = REPO / "game"

DESCRIPTION_FORMAT = 1

OVERLAY_BASES = {"us": 0x8004FC70, "jp": 0x8004FDD8}
OVERLAY_HEADER_BYTES = 1
TITLE_FILE = "BIN/TITLE.BIN"
FIELD_FILE = "BIN/FIELD.BIN"
HERO_TEMPLATES = ("g_hero_template_type_0", "g_hero_template_type_1")

SAVE_NAME_PREFIXES = {"SLUS_010.13": "BASLUS-01013", "SLPS_021.70": "BISLPS-02170"}
SAVE_NAME_PREFIX = SAVE_NAME_PREFIXES["SLUS_010.13"]
VERSIONS = {"SLUS_010.13": "us", "SLPS_021.70": "jp"}
CARD_SLOT_FOLDER = "slot0"

CARD_MAGIC = b"SC"
CARD_ICON_ONE_FRAME = 0x11
CARD_BLOCK_BYTES = 8192

VARIABLE_KIND_SHIFT = 12
VARIABLE_KIND_MASK = 7
VARIABLE_LOCATION_MASK = 0xFFF
VARIABLE_SAVED_KINDS = 3
WORD_BITS = 32


def read_layout(layout_tool: Path) -> dict:
    """Run lom-save-layout and return the layout it prints."""
    return json.loads(subprocess.run([str(layout_tool)], check=True, capture_output=True, text=True).stdout)


def disc_version(disc: Path) -> str:
    """Which game the disc is: "us", or "jp" when it has the Japanese executable."""
    for executable, version in VERSIONS.items():
        if (disc / executable).is_file():
            return version
    return "us"


def overlay_offset(disc: Path, unit: str, name: str) -> int:
    """Where one of an overlay's symbols is in its decompressed file, for the game on the disc."""
    version = disc_version(disc)
    symbols = DECOMP / "config" / version / "symbols" / f"{unit}_symbol_addrs.txt"
    return symbol_address(symbols, name) - OVERLAY_BASES[version] + OVERLAY_HEADER_BYTES


def symbol_address(symbols: Path, name: str) -> int:
    """Read one symbol's address from a decomp symbol file."""
    match = re.search(rf"^{re.escape(name)}\s*=\s*(0x[0-9A-Fa-f]+);", symbols.read_text(), re.MULTILINE)
    if not match:
        raise SystemExit(f"lom_saves: {name} isn't in {symbols}")
    return int(match.group(1), 16)


_overlay_lock = threading.Lock()


@functools.lru_cache(maxsize=None)
def _read_overlay(disc: Path, name: str) -> bytes:
    """read_overlay's work, cached. Use read_overlay, which lets one thread do it at a time."""
    if str(DECOMP) not in sys.path:
        sys.path.insert(0, str(DECOMP))
    from tools.compression.decompress import decompress

    return decompress((disc / name).read_bytes())


def read_overlay(disc: Path, name: str) -> bytes:
    """Read and decompress an overlay from the disc, once per process (it takes about 0.3 s)."""
    with _overlay_lock:
        return _read_overlay(disc, name)


def new_game(disc: Path, layout: dict, hero: int) -> bytearray:
    """The saved game right after New Game: the template, then the chosen hero's record in party slot 0."""
    title = read_overlay(disc, TITLE_FILE)
    start = overlay_offset(disc, "title", "g_new_game_template")
    saved = bytearray(title[start : start + layout["saved_game_bytes"]])
    hero_start = overlay_offset(disc, "title", HERO_TEMPLATES[hero])
    characters = layout["fields"]["characters"][0]
    saved[characters : characters + layout["character_bytes"]] = title[hero_start : hero_start + layout["character_bytes"]]
    return saved


def variable_widths(disc: Path) -> list[int]:
    """Bits per variable kind, from FIELD's own table (32 or more means the whole word)."""
    field = read_overlay(disc, FIELD_FILE)
    start = overlay_offset(disc, "field", "g_field_script_var_widths")
    return list(field[start : start + 8])


def set_field(saved: bytearray, layout: dict, name: str, value: int) -> None:
    """Write a little-endian field of the saved game by name."""
    offset, size = layout["fields"][name]
    saved[offset : offset + size] = (value & ((1 << (8 * size)) - 1)).to_bytes(size, "little")


def set_variable(saved: bytearray, layout: dict, widths: list[int], reference: int, value: int) -> None:
    """Write a script variable the way the game's scripts do."""
    kind = (reference >> VARIABLE_KIND_SHIFT) & VARIABLE_KIND_MASK
    if kind >= VARIABLE_SAVED_KINDS:
        raise SystemExit(f"lom_saves: variable {reference:#x} lives in the field runtime, not the save")
    location = reference & VARIABLE_LOCATION_MASK
    word, shift = location >> 5, location & 31
    bits = min(widths[kind], WORD_BITS)
    mask = ((1 << bits) - 1) << shift
    offset = layout["fields"]["words"][0] + 4 * word
    current = struct.unpack_from("<I", saved, offset)[0]
    struct.pack_into("<I", saved, offset, (current & ~mask) | ((value << shift) & mask))


def get_variable(saved: bytes, layout: dict, widths: list[int], reference: int) -> int:
    """Read a script variable of the saved game the way the game's scripts do (the reverse of set_variable)."""
    kind = (reference >> VARIABLE_KIND_SHIFT) & VARIABLE_KIND_MASK
    if kind >= VARIABLE_SAVED_KINDS:
        raise SystemExit(f"lom_saves: variable {reference:#x} lives in the field runtime, not the save")
    location = reference & VARIABLE_LOCATION_MASK
    word, shift = location >> 5, location & 31
    bits = min(widths[kind], WORD_BITS)
    current = struct.unpack_from("<I", saved, layout["fields"]["words"][0] + 4 * word)[0]
    return (current >> shift) & ((1 << bits) - 1)


LAND_COUNT = 64
LAND_CELL, LAND_EXTRA, LAND_ORDER = 1, 2, 3
TRACK_LAND_MASK = (1 << 18) - 1
LOGIC_BLOCK_UNASSIGNED = 3
PET_STATUS_EGG = 0x80000000


def land_record(saved: bytes, layout: dict, index: int) -> dict:
    """One land record of the saved game: flags, x, z, extra and order."""
    offset, size = layout["fields"]["lands"]
    at = offset + index * (size // LAND_COUNT)
    return {"flags": saved[at], "x": saved[at + LAND_CELL] & 0xF, "z": saved[at + LAND_CELL] >> 4,
            "extra": saved[at + LAND_EXTRA], "order": saved[at + LAND_ORDER]}


def current_land(saved: bytes, layout: dict) -> int:
    """The land the saved game is in (the track word's low bits)."""
    offset, size = layout["fields"]["track"]
    return int.from_bytes(saved[offset : offset + size], "little") & TRACK_LAND_MASK


def land_distance(saved: bytes, layout: dict, index: int) -> int:
    """What FIELD's field_get_land_distance gives for a land: its map distance from land 0, plus its extra byte."""
    land, home = land_record(saved, layout, index), land_record(saved, layout, 0)
    return land["extra"] + abs(land["x"] - home["x"]) + abs(land["z"] - home["z"])


def apply_description(saved: bytearray, layout: dict, widths: list[int], description: dict) -> None:
    """Apply a save description's edits to the saved game."""
    set_field(saved, layout, "compatibility_tag", layout["tag_any"])
    scene = description.get("scene", {})
    for key, field in (("scene", "scene_id"), ("object", "object_id"), ("music", "music_id"), ("sound_bank", "sound_bank_id"),
                       ("secondary_music", "secondary_music_id"), ("track", "track"), ("options", "options")):
        if key in scene:
            set_field(saved, layout, field, scene[key])
    if "money" in description:
        set_field(saved, layout, "money", description["money"])
    for reference, value in description.get("variables", {}).items():
        set_variable(saved, layout, widths, int(reference, 0), value)
    if description.get("lands"):
        offset, size = layout["fields"]["lands"]
        for index, land in description["lands"].items():
            at = offset + int(index) * (size // LAND_COUNT)
            if "flags" in land:
                saved[at] = land["flags"]
            if "x" in land or "z" in land:
                x, z = land.get("x", saved[at + LAND_CELL] & 0xF), land.get("z", saved[at + LAND_CELL] >> 4)
                saved[at + LAND_CELL] = (x & 0xF) | ((z & 0xF) << 4)
            if "extra" in land:
                saved[at + LAND_EXTRA] = land["extra"]
            if "order" in land:
                saved[at + LAND_ORDER] = land["order"]
    if "world_map_events" in description:
        offset, size = layout["fields"]["flag_bits"]
        bits = sum(1 << event for event in description["world_map_events"])
        saved[offset : offset + size] = bits.to_bytes(size, "little")
    if "current_land" in description:
        offset, size = layout["fields"]["track"]
        track = int.from_bytes(saved[offset : offset + size], "little")
        saved[offset : offset + size] = ((track & ~TRACK_LAND_MASK) | description["current_land"]).to_bytes(size, "little")
    if description.get("encyclopedia") == "all":
        offset, size = layout["fields"]["encyclopedia_bits"]
        saved[offset : offset + size] = b"\xff" * size
    if description.get("logic_blocks"):
        offset, size = layout["fields"]["logic_blocks"]
        blocks = description["logic_blocks"][: size // 4]
        for index, (block_id, level, shape, *placed) in enumerate(blocks):
            word = (block_id & 0x3F) << 2 | (level & 0xF) << 8 | (shape & 0xF) << 12
            if placed:
                logic_type, rotation, x, y = placed
                word |= logic_type & 3 | 1 << 16 | (rotation & 3) << 17 | (x & 0x1F) << 19 | (y & 0x1F) << 24
            else:
                word |= LOGIC_BLOCK_UNASSIGNED
            saved[offset + 4 * index : offset + 4 * index + 4] = word.to_bytes(4, "little")
        count_offset, _ = layout["fields"]["logic_block_count"]
        saved[count_offset] = len(blocks)
    for index in description.get("hatched_pets", []):
        offset, _ = layout["fields"]["pets"]
        at = offset + int(index) * layout["pet_bytes"]
        status = at + layout["pet_status_offset"]
        word = int.from_bytes(saved[status : status + 4], "little") | PET_STATUS_EGG
        saved[status : status + 4] = word.to_bytes(4, "little")
        hatch = at + layout["pet_hatch_counter_offset"]
        saved[hatch : hatch + 2] = b"\0\0"
    if description.get("item_counts"):
        offset, _ = layout["fields"]["item_counts"]
        for kind, count in description["item_counts"].items():
            saved[offset + int(kind, 0)] = count
    words_offset = layout["fields"]["words"][0]
    for index, value in description.get("words", {}).items():
        struct.pack_into("<I", saved, words_offset + 4 * int(index, 0), int(value, 0) if isinstance(value, str) else value)


def save_file(saved: bytes, layout: dict, title: str) -> bytes:
    """Wrap a saved game in a card file: header, saved game, checksum and magic."""
    data = bytearray(layout["save_file_bytes"])
    data[0:2] = CARD_MAGIC
    data[2] = CARD_ICON_ONE_FRAME
    data[3] = layout["save_file_bytes"] // CARD_BLOCK_BYTES
    encoded = title.encode("ascii")[: layout["card_title_line_bytes"]]
    data[4 : 4 + len(encoded)] = encoded
    start = layout["saved_game_offset"]
    data[start : start + len(saved)] = saved
    covered = layout["checksum_bytes"]
    checksum = sum(data[:covered]) * layout["checksum_multiplier"] + layout["checksum_bias"]
    struct.pack_into("<iI", data, layout["checksum_offset"], checksum, layout["magic"])
    return bytes(data)


ROUTES_ROOT = Path(__file__).resolve().parent.parent.parent / "tests" / "routes"


def save_name_prefix(disc: Path | None) -> str:
    """The start of the game's save file names: the US game's, or the Japanese one's when the disc has its
    executable. The two keep their saves the same way otherwise."""
    for executable, prefix in SAVE_NAME_PREFIXES.items():
        if disc is not None and (disc / executable).is_file():
            return prefix
    return SAVE_NAME_PREFIX


def save_name(serial: int, suffix: int, disc: Path | None = None) -> str:
    """The card file name for a save, for the game on the disc (the US one when there's none)."""
    return f"{save_name_prefix(disc)}{serial:X}-{suffix:X}"


def load_description(path: Path) -> dict:
    """Read a save description. A "card" (a card file the game wrote at the end of a route, like
    "saves/after-opening.card") is looked for next to the description and then in the folders above it."""
    return find_files(json.loads(path.read_text()), path.parent, str(path))


def find_files(description: dict, folder: Path, where: str) -> dict:
    """Find a description's card file or saved game in a folder or the folders above it, as load_description
    does for one in a file. Where names the description in an error."""
    folders = [folder, *folder.parents, ROUTES_ROOT]
    if "saved_game" in description:
        found = next((above / description["saved_game"] for above in folders
                      if (above / description["saved_game"]).exists()), None)
        if found is None:
            raise SystemExit(f"lom_saves: {where}: no saved game {description['saved_game']} next to it or above it")
        description["_saved_game_path"] = str(found)
    if "card" in description:
        found = next((above / description["card"] for above in folders if (above / description["card"]).exists()), None)
        if found is None:
            raise SystemExit(f"lom_saves: {where}: no card file {description['card']} next to it or above it")
        description["_card_path"] = str(found)
    return description


def saved_game(description: dict, disc: Path, layout: dict, widths: list[int] | None = None) -> bytearray:
    """The saved game a description describes: a new game, or the saved game in its card file (one the game
    wrote), with the description's edits applied."""
    if "_card_path" in description:
        data = Path(description["_card_path"]).read_bytes()
        start = layout["saved_game_offset"]
        saved = bytearray(data[start : start + layout["saved_game_bytes"]])
    elif "_saved_game_path" in description:
        saved = bytearray(Path(description["_saved_game_path"]).read_bytes()[: layout["saved_game_bytes"]])
    elif "saved_game_base64" in description:
        saved = bytearray(base64.b64decode(description["saved_game_base64"])[: layout["saved_game_bytes"]])
    else:
        saved = new_game(disc, layout, description.get("hero", 0))
    apply_description(saved, layout, widths if widths is not None else variable_widths(disc), description)
    return saved


def card_of(description: dict) -> dict:
    """The keys a description built from this one has to keep to start from the same card file."""
    return {key: description[key] for key in ("card", "saved_game") if key in description}


def build(description: dict, disc: Path, layout: dict) -> bytes:
    """Build a whole save file from a description."""
    if description.get("format") != DESCRIPTION_FORMAT:
        raise SystemExit(f"lom_saves: unsupported save description format {description.get('format')}")
    return save_file(bytes(saved_game(description, disc, layout)), layout, description["description"])


def read(data: bytes, layout: dict) -> dict:
    """Read a save file into plain data: whether it's valid, and its fields and variables."""
    covered = layout["checksum_bytes"]
    stored, magic = struct.unpack_from("<iI", data, layout["checksum_offset"])
    checksum = sum(data[:covered]) * layout["checksum_multiplier"] + layout["checksum_bias"]
    saved = data[layout["saved_game_offset"] :]
    fields = {}
    for name, (offset, size) in layout["fields"].items():
        if size in (1, 2, 4):
            fields[name] = int.from_bytes(saved[offset : offset + size], "little")
    words_offset, words_size = layout["fields"]["words"]
    words = list(struct.unpack_from(f"<{words_size // 4}I", saved, words_offset))
    return {"valid": stored == checksum and magic == layout["magic"], "checksum_ok": stored == checksum, "magic_ok": magic == layout["magic"],
            "fields": fields, "words": words}


def main() -> int:
    """Read the command line, then build a save or read one."""
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    build_command = commands.add_parser("build", help="write a save from a description")
    build_command.add_argument("description", type=Path)
    build_command.add_argument("--disc", type=Path, required=True)
    build_command.add_argument("--layout", type=Path, required=True, help="the lom-save-layout program")
    build_command.add_argument("--card-root", type=Path, required=True, help="card folder; the save goes in its slot0/")
    build_command.add_argument("--serial", type=int, default=1)
    read_command = commands.add_parser("read", help="print a save as JSON")
    read_command.add_argument("save", type=Path)
    read_command.add_argument("--layout", type=Path, required=True)
    args = parser.parse_args()

    layout = read_layout(args.layout)
    if args.command == "build":
        data = build(json.loads(args.description.read_text()), args.disc, layout)
        folder = args.card_root / CARD_SLOT_FOLDER
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / save_name(args.serial, 1, args.disc)
        path.write_bytes(data)
        print(path)
    else:
        print(json.dumps(read(args.save.read_bytes(), layout), indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())

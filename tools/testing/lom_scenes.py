from __future__ import annotations

import functools
import hashlib
import json
import os
import re
import struct
import sys
import threading
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DECOMP = REPO / "game"
RESOURCE_PATHS = REPO / "src" / "cd" / "disc_resource_paths.inc"
CHANGE_SCENE_INDEX_FORMAT = 1

SCENE_RESOURCE_BASE = 1548
SCENE_COUNT = 930
MAP_RESOURCE_BASE = 180
MAP_LOAD_ADDRESS = 0x80180000
MAP_ID_MASK = 0x7FFF
SCENE_MASK = 0x7FFF
MAP_OBJECT_MARKERS = 0x0C
MAP_OBJECTS_POINTER = 0x20
LAYOUT_RECORD_BYTES = 48
LAYOUT_POSITION_OFFSET = 8
LAYOUT_X_MASK = 0xFFFF
LAYOUT_Z_MASK = 0x7FF
NO_SCRIPT = 0xFFFF
EVENT_SCRIPT = 0x8000
EVENT_SCRIPT_NUMBER = 0x7FFF

VARIABLE_WRITES = ("set_variable", "write_variable", "calculate")


def written_variable(instruction: dict) -> int | None:
    """The variable a decoded script instruction writes, or None if it writes none."""
    if instruction["command"] not in VARIABLE_WRITES:
        return None
    return instruction["operands"]["destination"].get("reference")


def _decomp_tools():
    """Make the decomp's scene tools importable (tools.data.scenes and friends)."""
    if str(DECOMP) not in sys.path:
        sys.path.insert(0, str(DECOMP))


@functools.lru_cache(maxsize=None)
def resource_paths() -> dict[int, str]:
    """CD resource number to path on the disc."""
    rows = re.findall(r'\{0x([0-9A-F]+), "([^"]+)", (\d+)\}', RESOURCE_PATHS.read_text())
    return {int(number, 16): path for number, path, _ in rows}


@dataclass(frozen=True)
class Marker:
    label: int
    center_x: int
    center_z: int
    targets: tuple[int, ...]


class Scene:
    def __init__(self, disc: Path, number: int):
        """Read and decode scene number from the disc."""
        _decomp_tools()
        from tools.data.scenes.scene_format import SceneHeader
        from tools.data.scenes.event_scripts import read_event_scripts
        from tools.data.scenes.script_links import EventLinks

        self.disc = disc
        self.number = number
        self.data = (disc / resource_paths()[SCENE_RESOURCE_BASE + number]).read_bytes()
        self.header = SceneHeader.parse(self.data)
        self.scripts = read_event_scripts(self.data, self.header)
        self.links = EventLinks(self.scripts)

    def map_id(self) -> int:
        """The scene's map file number."""
        return struct.unpack_from("<4H", self.data, self.header.actors)[2] & MAP_ID_MASK

    def change_scenes(self, entry: int) -> list[dict]:
        """The change_scene instructions an event script can reach, with their operands."""
        out = []
        for instruction in self.links.resolve(entry)["instructions"]:
            if instruction["command"] == "change_scene":
                out.append({name: operand.get("value") for name, operand in instruction["operands"].items()})
        return out

    def markers(self, map_object: int) -> list[Marker]:
        """The walk-in markers of one map object, with where their scripts lead."""
        out = []
        for x0, y0, x1, y1, offset_x, offset_y, label in _map_markers(self.disc, self.map_id(), map_object):
            xs = (x0, x1, x0 + offset_x, x1 + offset_x)
            zs = (y0, y1, y0 + offset_y, y1 + offset_y)
            targets = tuple(sorted({change["scene_id"] & SCENE_MASK for change in self.change_scenes(label) if change.get("scene_id") is not None}))
            out.append(Marker(label, sum(xs) // 4, sum(zs) // 4, targets))
        return out

    def layout_position(self, record: int) -> tuple[int, int]:
        """Where a layout record places its actor, in map x and z."""
        count = struct.unpack_from("<I", self.data, self.header.layout)[0]
        if not 0 <= record < count:
            raise ValueError(f"scene {self.number} has no layout record {record}")
        position = struct.unpack_from("<I", self.data, self.header.layout + 4 + record * LAYOUT_RECORD_BYTES + LAYOUT_POSITION_OFFSET)[0]
        return position & LAYOUT_X_MASK, (position >> 16) & LAYOUT_Z_MASK


def event_script(slot: int) -> int | None:
    """The event script a layout record's script slot runs, or None for an empty slot or a message."""
    return slot & EVENT_SCRIPT_NUMBER if slot != NO_SCRIPT and slot & EVENT_SCRIPT else None


@functools.lru_cache(maxsize=None)
def scene(disc: Path, number: int) -> Scene:
    """A scene, read once."""
    return Scene(disc, number)


@functools.lru_cache(maxsize=None)
def _map_file(disc: Path, map_id: int) -> bytes:
    """A map file from the disc, decompressed, read once."""
    _decomp_tools()
    from tools.compression.decompress import decompress

    path = resource_paths()[MAP_RESOURCE_BASE + map_id]
    raw = (disc / path).read_bytes()
    return decompress(raw[1:]) if path.upper().endswith(".PRS") else raw


def _map_markers(disc: Path, map_id: int, map_object: int) -> list[tuple[int, ...]]:
    """FieldMarkerDef records of one map object: (x0, y0, x1, y1, offset x, offset y, label)."""
    data = _map_file(disc, map_id)
    word = lambda offset: struct.unpack_from("<I", data, offset)[0]
    objects = word(MAP_OBJECTS_POINTER) - MAP_LOAD_ADDRESS
    count = 0
    while word(objects + 4 * count):
        count += 1
    map_object = min(map_object, count - 1)
    pointer = word(word(objects + 4 * map_object) - MAP_LOAD_ADDRESS + MAP_OBJECT_MARKERS)
    out = []
    while pointer:
        offset = pointer - MAP_LOAD_ADDRESS
        x0, y0, x1, y1 = struct.unpack_from("<4H", data, offset + 4)
        offset_x, offset_y = struct.unpack_from("<2h", data, offset + 12)
        label = struct.unpack_from("<H", data, offset + 20)[0]
        out.append((x0, y0, x1, y1, offset_x, offset_y, label))
        pointer = word(offset)
    return out


def _index_key(disc: Path) -> str:
    """Identifies the scene files (by size and time) and the code that decodes them."""
    digest = hashlib.sha256(f"{CHANGE_SCENE_INDEX_FORMAT} {disc.resolve()}\n".encode())
    for source in sorted([Path(__file__), *(DECOMP / "tools" / "data" / "scenes").glob("*.py")]):
        digest.update(source.read_bytes())
    for number in range(SCENE_COUNT):
        info = (disc / resource_paths()[SCENE_RESOURCE_BASE + number]).stat()
        digest.update(f"{number} {info.st_size} {info.st_mtime_ns}\n".encode())
    return digest.hexdigest()[:16]


_index_lock = threading.Lock()


@functools.lru_cache(maxsize=None)
def _change_scene_index(disc: Path) -> list[dict]:
    """change_scene_index's work, cached. Use change_scene_index, which lets one thread do it at a time."""
    cache = Path(os.environ.get("XDG_CACHE_HOME") or Path.home() / ".cache") / "lom-native" / f"change-scenes-{_index_key(disc)}.json"
    if cache.exists():
        return json.loads(cache.read_text())
    out = []
    for number in range(SCENE_COUNT):
        for instruction in scene(disc, number).scripts["instructions"]:
            if instruction["command"] == "change_scene":
                operands = {name: operand.get("value") for name, operand in instruction["operands"].items()}
                if operands.get("scene_id") is not None:
                    out.append({"from": number, **operands})
    cache.parent.mkdir(parents=True, exist_ok=True)
    partial = cache.with_name(f"{cache.name}.{os.getpid()}")
    partial.write_text(json.dumps(out))
    partial.replace(cache)
    return out


def change_scene_index(disc: Path) -> list[dict]:
    """Every change_scene in the game, with the scene it's in.

    Decoding all 930 scenes takes several seconds, so the list is kept in the
    user's cache folder, keyed by the scene files and the decoder's source.
    """
    with _index_lock:
        return _change_scene_index(disc)


def change_scenes_into(disc: Path, target: int) -> list[dict]:
    """Every change_scene in the game that enters a scene, with the scene it's in."""
    return [change for change in change_scene_index(disc) if change["scene_id"] & SCENE_MASK == target]

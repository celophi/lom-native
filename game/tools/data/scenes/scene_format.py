from __future__ import annotations

from dataclasses import dataclass
import struct
from typing import NamedTuple


SCENE_HEADER = struct.Struct("<10I")


@dataclass(frozen=True)
class SceneSection:
    name: str
    start: int
    end: int


class SceneHeader(NamedTuple):
    layout: int
    event_scripts: int
    strings: int
    actor_scripts: int
    records: int
    actors: int
    geometry: int
    images: int
    portraits: int
    group_bounds: int

    @classmethod
    def parse(cls, data: bytes) -> SceneHeader:
        """Read the header and check that its section boundaries fit the IMG."""
        if len(data) < SCENE_HEADER.size:
            raise ValueError("truncated scene header")
        header = cls(*SCENE_HEADER.unpack_from(data))
        if header.layout != SCENE_HEADER.size:
            raise ValueError("expected scene layout offset 0x28")
        if any(offset % 4 or offset > len(data) for offset in header):
            raise ValueError("section offsets must be aligned and within the file")
        if tuple(header) != tuple(sorted(header)):
            raise ValueError("section offsets must be in file order")
        return header

    def sections(self, file_size: int) -> list[SceneSection]:
        """Pair each section start with its following boundary."""
        return [
            SceneSection("layout", self.layout, self.event_scripts),
            SceneSection("event_scripts", self.event_scripts, self.strings),
            SceneSection("strings", self.strings, self.actor_scripts),
            SceneSection("actor_scripts", self.actor_scripts, self.records),
            SceneSection("records", self.records, self.actors),
            SceneSection("actors", self.actors, self.geometry),
            SceneSection("geometry", self.geometry, self.images),
            SceneSection("images", self.images, self.portraits),
            SceneSection("portraits", self.portraits, self.group_bounds),
            SceneSection("group_bounds", self.group_bounds, file_size),
        ]

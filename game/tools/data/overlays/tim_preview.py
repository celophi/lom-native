from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import struct

from tools.data.formats.psx_tim import TimImage
from tools.data.overlays import icon_set, png

PIXELS_PER_WORD = {0: 4, 1: 2, 2: 1}
PALETTE_COLORS = {0: 16, 1: 256}


@dataclass(frozen=True)
class Preview:
    file: str
    palette: int | None
    colors: tuple[int, ...]

    def document(self) -> dict[str, object]:
        if self.palette is None:
            return {"file": self.file, "note": "16-bit direct color"}
        return {"index": self.palette, "file": self.file, "colors": [f"0x{value:04X}" for value in self.colors]}


def width(tim: TimImage) -> int:
    """Image width in pixels."""
    return tim.pixels.width_words * PIXELS_PER_WORD[tim.pixel_mode]


def check(tim: TimImage, what: str) -> None:
    """Reject modes and palettes the previews can't show."""
    mode = tim.pixel_mode
    if mode not in PIXELS_PER_WORD:
        raise ValueError(f"{what} uses TIM pixel mode {mode}, which has no preview")
    if mode in PALETTE_COLORS:
        size = PALETTE_COLORS[mode] * 2
        if tim.clut is None or not tim.clut.payload or len(tim.clut.payload) % size:
            raise ValueError(f"{what} needs complete {PALETTE_COLORS[mode]}-color palettes")


def palettes(tim: TimImage) -> tuple[tuple[int, ...], ...]:
    """The stored palettes, in CLUT order; empty for a 16-bit image."""
    if tim.pixel_mode not in PALETTE_COLORS:
        return ()
    count = PALETTE_COLORS[tim.pixel_mode]
    return tuple(struct.iter_unpack(f"<{count}H", tim.clut.payload))


def pixel_values(tim: TimImage) -> list[int]:
    """Row-major palette indices, or BGR555 words for a 16-bit image."""
    payload = tim.pixels.payload
    if tim.pixel_mode == 0:
        return [nibble for byte in payload for nibble in (byte & 15, byte >> 4)]
    if tim.pixel_mode == 1:
        return list(payload)
    return [value for (value,) in struct.iter_unpack("<H", payload)]


def write_previews(folder: Path, tim: TimImage) -> list[Preview]:
    """Write palette_NN.png for each palette (or image.png for direct color) into @p folder."""
    check(tim, str(folder))
    folder.mkdir(parents=True, exist_ok=True)
    image_width, height = width(tim), tim.pixels.height
    values = pixel_values(tim)
    stored = palettes(tim)
    if not stored:
        png.write_rgba(folder / "image.png", image_width, height, [icon_set.bgr555_to_rgba(value) for value in values])
        return [Preview("image.png", None, ())]
    previews = []
    for index, palette in enumerate(stored):
        colors = [icon_set.bgr555_to_rgba(value) for value in palette]
        file = f"palette_{index:02X}.png"
        png.write_rgba(folder / file, image_width, height, [colors[value] for value in values])
        previews.append(Preview(file, index, palette))
    return previews

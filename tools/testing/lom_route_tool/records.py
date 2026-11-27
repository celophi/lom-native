from __future__ import annotations

import hashlib
import struct
from pathlib import Path

import lom_saves
import lom_scenes

from .settings import BASELINE_FORMAT, CHANNELS, COMPACT_FORMAT, SAVE_DUMP


PICTURE_DIGITS = 8
POSITION_SCALE = 256
FIRST_RECORD_ACTOR = 3
UNLISTED_SAVE_FIELDS = ("play_time",)
SAVE_INPUT_FIELDS = ("encyclopedia_bits", "logic_block_count", "logic_blocks", "pets")


def read_record(folder: Path) -> dict:
    """A run record as a baseline-shaped dict, with every frame line (format 1)."""
    record = {"format": 1, "frames": [], "screens": [], "checkpoints": [], "states": [], "notes": [], "end": None, "coverage": {}}
    frames_file = folder / "frames.txt"
    if not frames_file.exists():
        return record
    for line in frames_file.read_text().splitlines():
        kind, _, rest = line.partition(" ")
        if (kind in ("frame", "screen") and not rest.split(" ", 1)[0].isdigit()) or kind not in ("frame", "screen", "checkpoint", "state", "note", "end"):
            record.setdefault("damaged", line)
        elif kind == "frame":
            record["frames"].append(rest)
        elif kind == "screen":
            record["screens"].append(rest)
        elif kind == "checkpoint":
            record["checkpoints"].append(rest)
        elif kind == "state":
            record["states"].append(rest)
        elif kind == "note":
            record["notes"].append(rest)
        elif kind == "end":
            record["end"] = rest
    coverage_file = folder / "coverage.txt"
    if coverage_file.exists():
        for line in coverage_file.read_text().splitlines():
            kind = line.split()[0]
            record["coverage"][kind] = record["coverage"].get(kind, 0) + 1
    return record


def frame_fields(line: str) -> dict[str, str]:
    """A frame line's channels by name."""
    words = line.split()
    fields = {"frame": words[0]}
    fields.update(zip(words[1::2], words[2::2]))
    return fields


def compact_frames(frames: list[str], checkpoints: list[str]) -> dict:
    """A run's frame lines in format 2: each frame's picture prefix, and a digest of each stretch of whole lines.

    A stretch ends with a checkpoint's frame; the last one runs to the end.
    """
    ends = [int(line.split()[2]) for line in checkpoints if line.split()[1:2] == ["frame"]]
    stretches, start = [], 0
    for end in [*ends, None]:
        lines = [line for line in frames[start:] if end is None or int(line.split()[0]) <= end]
        start += len(lines)
        stretches.append({"frames": len(lines), "sha256": hashlib.sha256("\n".join(lines).encode()).hexdigest()[:32]})
        if end is None:
            break
    pictures = " ".join(frame_fields(line).get("picture", "-")[:PICTURE_DIGITS] for line in frames)
    return {"pictures": pictures, "stretches": stretches}


def compact_screens(screens: list[str]) -> str:
    """A run's screen lines, the vblanks something was drawn while FIELD wasn't (the world map, menus of their own), as
    each one's picture prefix, with a run of the same one written once as prefix*count."""
    prefixes = [frame_fields(line).get("picture", "-")[:PICTURE_DIGITS] for line in screens]
    out: list[str] = []
    for prefix in prefixes:
        if out and out[-1].split("*")[0] == prefix:
            word, _, count = out[-1].partition("*")
            out[-1] = f"{word}*{int(count or 1) + 1}"
        else:
            out.append(prefix)
    return " ".join(out)


def expand_screens(compact: str) -> list[str]:
    """compact_screens() undone: each screen's picture prefix."""
    out: list[str] = []
    for word in compact.split():
        prefix, _, count = word.partition("*")
        out += [prefix] * int(count or 1)
    return out


def as_compact(record: dict) -> dict:
    """A baseline or run in format 2."""
    if record.get("format", 1) >= 2:
        return record
    return {**{k: v for k, v in record.items() if k != "frames"}, "format": COMPACT_FORMAT, **compact_frames(record["frames"], record["checkpoints"])}


def checkpoint_states(run: dict) -> list[dict]:
    """The state lines of each checkpoint, decoded: scene words, actors and battle records by number."""
    groups: list[list[str]] = []
    previous = None
    for line in run["states"]:
        name = line.split(" ", 1)[0]
        if name != previous:
            groups.append([])
            previous = name
        groups[-1].append(line.split(" ", 1)[1])
    states = []
    for lines in groups:
        state = {"words": {}, "actors": {}, "battles": {}}
        for line in lines:
            words = line.split()
            if words[0] == "variables":
                for offset, value in enumerate(words[2:]):
                    state["words"][int(words[1]) + offset] = int(value, 16)
            elif words[0] == "actor":
                state["actors"][int(words[1])] = dict(zip(words[2::2], words[3::2]))
            elif words[0] == "battle":
                state["battles"][int(words[1])] = dict(zip(words[2::2], words[3::2]))
        states.append(state)
    return states


def actor_name(index: int) -> str:
    """How the change list names an actor."""
    if index == 0:
        return "actor 0 (the hero)"
    if index >= FIRST_RECORD_ACTOR:
        return f"actor {index} (layout record {index - FIRST_RECORD_ACTOR})"
    return f"actor {index}"


def map_position(text: str) -> str:
    """An actor's x,y,z to the nearest map unit, as (x, z), with the height only when it isn't 0. A move of
    less than a unit doesn't show; the state fingerprint still catches it."""
    x, y, z = (round(int(value) / POSITION_SCALE) for value in text.split(","))
    return f"({x}, {z})" + (f" at height {y}" if y else "")


def actor_changes(before: dict, after: dict) -> list[str]:
    """What changed among the actors, one line each. With no before, where each one is."""
    lines = []
    for index in sorted(set(before) | set(after)):
        name = actor_name(index)
        if index not in after:
            lines.append(f"{name}: gone")
        elif index not in before:
            actor = after[index]
            lines.append(f"{name}: at {map_position(actor['position'])}, facing {actor['direction']}, animation {actor['animation']}, "
                         f"presence {actor['presence']}, control {actor['control']}, command {actor['command']}")
        else:
            old, new = before[index], after[index]
            parts = []
            if map_position(old["position"]) != map_position(new["position"]):
                parts.append(f"moved {map_position(old['position'])} to {map_position(new['position'])}")
            parts += [f"{key} {old[key]} to {new[key]}" for key in ("direction", "animation", "presence", "control", "command") if old[key] != new[key]]
            if parts:
                lines.append(f"{name}: {'; '.join(parts)}")
    return lines


def battle_changes(before: dict, after: dict) -> list[str]:
    """What changed among the battle records, in plain lines."""
    lines = []
    for index in sorted(set(before) | set(after)):
        if index not in after:
            lines.append(f"battle record {index} is gone")
        elif index not in before:
            record = after[index]
            lines.append(f"battle record {index}: monster {record['monster']}, side {record['side']}, hp {record['hp']}")
        elif before[index] != after[index]:
            old, new = before[index], after[index]
            lines += [f"battle record {index} (monster {new['monster']}) {key} {old[key]} to {new[key]}" for key in ("monster", "side", "hp")
                      if old[key] != new[key]]
    return lines


def word_change(kind: str, index: int, old: int, new: int, first_bit: int | None = None) -> str:
    """One changed 32-bit word, with the bits set and cleared. For a story word, the bits are named by the
    numbers the scripts' variable references use (a reference's low 12 bits are its first bit)."""
    line = f"{kind} word {index}: {old:08x} to {new:08x}"
    if first_bit is None:
        return line
    set_bits = [f"{first_bit + bit:#x}" for bit in range(32) if (new >> bit) & 1 and not (old >> bit) & 1]
    cleared = [f"{first_bit + bit:#x}" for bit in range(32) if (old >> bit) & 1 and not (new >> bit) & 1]
    named = [f"{label} {', '.join(bits)}" for label, bits in (("set", set_bits), ("cleared", cleared)) if bits]
    return line + (f" ({'; '.join(named)})" if named else "")


def story_word_changes(before: bytes, after: bytes, offset: int, size: int) -> list[str]:
    """Each story word that changed, with the flags in it that did."""
    lines = []
    for index in range(size // 4):
        old_word, new_word = (struct.unpack_from("<I", data, offset + 4 * index)[0] for data in (before, after))
        if old_word != new_word:
            lines.append(word_change("story", index, old_word, new_word, 32 * index))
    return lines


def land_changes(before: bytes, after: bytes, layout: dict, old: bytes, new: bytes) -> list[str]:
    """Each land's record fields that changed, or that its spirit levels did when nothing else in it changed."""
    lines = []
    record = len(old) // lom_saves.LAND_COUNT
    for index in range(lom_saves.LAND_COUNT):
        old_land, new_land = lom_saves.land_record(before, layout, index), lom_saves.land_record(after, layout, index)
        lines += [f"land {index} {key} {old_land[key]} to {new_land[key]}" for key in old_land if old_land[key] != new_land[key]]
        if old_land == new_land and old[index * record : (index + 1) * record] != new[index * record : (index + 1) * record]:
            lines.append(f"land {index}'s spirit levels changed")
    return lines


def character_changes(old: bytes, new: bytes, record: int) -> list[str]:
    """How many bytes of each character's record changed."""
    lines = []
    for index in range(len(old) // record):
        changed = sum(a != b for a, b in zip(old[index * record : (index + 1) * record], new[index * record : (index + 1) * record]))
        if changed:
            lines.append(f"character {index}: {changed} bytes changed")
    return lines


def field_change(name: str, old: bytes, new: bytes) -> str:
    """A field that changed: a name as text, a number as a number, anything else as hex."""
    label = name.replace("_", " ")
    if name == "summary_name":
        old_name, new_name = (text.split(b"\0")[0].decode("latin-1") for text in (old, new))
        return f"{label} {old_name!r} to {new_name!r}"
    if len(old) in (1, 2, 4):
        return f"{label} {int.from_bytes(old, 'little')} to {int.from_bytes(new, 'little')}"
    return f"{label} {old.hex()} to {new.hex()}"


def save_changes(before: bytes, after: bytes, layout: dict) -> list[str]:
    """What changed in the saved game, in plain lines: named fields, story words, lands, characters, then anything else."""
    lines = []
    named = bytearray(len(after))
    for name, (offset, size) in layout["fields"].items():
        if name in SAVE_INPUT_FIELDS:
            continue
        named[offset : offset + size] = b"\1" * size
        old, new = before[offset : offset + size], after[offset : offset + size]
        if old == new or name in UNLISTED_SAVE_FIELDS:
            continue
        if name == "words":
            lines += story_word_changes(before, after, offset, size)
        elif name == "lands":
            lines += land_changes(before, after, layout, old, new)
        elif name == "characters":
            lines += character_changes(old, new, layout["character_bytes"])
        else:
            lines.append(field_change(name, old, new))
    other = [offset for offset in range(len(after)) if not named[offset] and before[offset] != after[offset]]
    if other:
        lines.append(f"{len(other)} other saved-game {'byte' if len(other) == 1 else 'bytes'} changed, the first at offset {other[0]}")
    return lines


def scene_name(number: int) -> str:
    """A scene's file name, like HTW_INN0, or its number when it has none."""
    path = lom_scenes.resource_paths().get(lom_scenes.SCENE_RESOURCE_BASE + number)
    return f"scene {number} ({Path(path).stem})" if path and 0 <= number < lom_scenes.SCENE_COUNT else f"scene {number}"


def describe_changes(run: dict, record_folder: Path, start: bytes, layout: dict) -> list[dict]:
    """What changed at each checkpoint since the one before, in plain lines (the first since the starting save,
    with every actor's place)."""
    states = checkpoint_states(run)
    out = []
    previous_save, previous_state = start, {"words": {}, "actors": {}, "battles": {}}
    for index, line in enumerate(run["checkpoints"]):
        words = line.split()
        lines = [f"frame {words[2]}, {scene_name(int(words[4]))}"]
        save_path = record_folder / SAVE_DUMP.format(index)
        if save_path.exists():
            save = save_path.read_bytes()
            lines += save_changes(previous_save, save, layout)
            previous_save = save
        else:
            lines.append("the saved game wasn't written at this checkpoint")
        state = states[index] if index < len(states) else {"words": {}, "actors": {}, "battles": {}}
        if index:
            lines += [word_change("scene", word, previous_state["words"].get(word, 0), value)
                      for word, value in sorted(state["words"].items()) if previous_state["words"].get(word, 0) != value]
        lines += actor_changes(previous_state["actors"], state["actors"])
        lines += battle_changes(previous_state["battles"], state["battles"])
        out.append({"checkpoint": words[0], "changes": lines})
        previous_state = state
    return out


def as_baseline(route_id: str, description: str, plan_text: str, run: dict) -> dict:
    """A run as a format 3 baseline: the fingerprints and the change lists, without the raw state."""
    compact = as_compact({"route": route_id, "description": description, "plan": plan_text,
                          **{key: value for key, value in run.items() if key not in ("states", "changes")}})
    baseline = {"route": compact["route"], "description": compact["description"], "plan": compact["plan"], "format": BASELINE_FORMAT,
                "checkpoints": compact["checkpoints"], "changes": run["changes"], "notes": compact["notes"], "end": compact["end"],
                "coverage": compact["coverage"], "pictures": compact["pictures"], "stretches": compact["stretches"]}
    if run.get("screens"):
        baseline["screens"] = compact_screens(run["screens"])
    return baseline


def compare(baseline: dict, run: dict) -> list[str]:
    """Where a run differs from its baseline: the first difference first, then a summary. Empty when they match."""
    if baseline.get("format", 1) >= 2:
        return compare_compact(baseline, run)
    differences = []
    for index, (expected, actual) in enumerate(zip(baseline["frames"], run["frames"])):
        if expected != actual:
            want, got = frame_fields(expected), frame_fields(actual)
            channels = [c for c in CHANNELS if want.get(c) != got.get(c)]
            differences.append(f"frame {want['frame']}: {', '.join(channels)} differ"
                               + "".join(f"\n    {c}: baseline {want.get(c)}, this run {got.get(c)}" for c in channels))
            break
    if len(baseline["frames"]) != len(run["frames"]):
        differences.append(f"the run has {len(run['frames'])} frames, the baseline {len(baseline['frames'])}")
    if differences:
        counts = {c: 0 for c in CHANNELS}
        for expected, actual in zip(baseline["frames"], run["frames"]):
            want, got = frame_fields(expected), frame_fields(actual)
            for c in CHANNELS:
                counts[c] += want.get(c) != got.get(c)
        differences.append("frames differing by channel: " + ", ".join(f"{c} {n}" for c, n in counts.items()))
    for expected, actual in zip(baseline["checkpoints"], run["checkpoints"]):
        if expected != actual:
            differences.append(f"checkpoint differs:\n    baseline {expected}\n    this run {actual}")
    if len(baseline["checkpoints"]) != len(run["checkpoints"]):
        differences.append(f"the run reached {len(run['checkpoints'])} checkpoints, the baseline {len(baseline['checkpoints'])}")
    for expected, actual in zip(baseline.get("states", []), run["states"]):
        if expected != actual:
            differences.append(f"game state differs at a checkpoint:\n    baseline {expected}\n    this run {actual}")
            break
    if baseline["notes"] != run["notes"]:
        differences.append("notes differ:\n    baseline " + "; ".join(baseline["notes"]) + "\n    this run " + "; ".join(run["notes"]))
    if baseline["end"] != run["end"]:
        differences.append(f"end differs: baseline {baseline['end']}, this run {run['end']}")
    return differences


def compare_compact(baseline: dict, run: dict) -> list[str]:
    """compare() for a format 2 baseline. The run still has every frame line, so the
    report can say which channels a differing picture frame or stretch has."""
    differences = []
    ours = compact_frames(run["frames"], run["checkpoints"])
    if "pictures" in baseline:
        want, got = baseline["pictures"].split(), ours["pictures"].split()
        first = next((i for i, (a, b) in enumerate(zip(want, got)) if a != b), None)
        if first is not None:
            differences.append(f"frame {first}: the picture differs (baseline {want[first]}, this run {got[first]}); "
                               f"{sum(a != b for a, b in zip(want, got))} frames' pictures differ")
    if "screens" in baseline:
        want, got = expand_screens(baseline["screens"]), expand_screens(compact_screens(run.get("screens", [])))
        first = next((i for i, (a, b) in enumerate(zip(want, got)) if a != b), None)
        if first is not None:
            vblank = run["screens"][first].split(" ", 1)[0]
            differences.append(f"screen {first} (vblank {vblank}): the picture differs (baseline {want[first]}, this run {got[first]}); "
                               f"{sum(a != b for a, b in zip(want, got))} screens' pictures differ")
        if len(want) != len(got):
            differences.append(f"the run has {len(got)} screens outside FIELD, the baseline {len(want)}")
    want_frames, got_frames = sum(stretch["frames"] for stretch in baseline["stretches"]), len(run["frames"])
    if want_frames != got_frames:
        differences.append(f"the run has {got_frames} frames, the baseline {want_frames}")
    for index, (expected, actual) in enumerate(zip(baseline["stretches"], ours["stretches"])):
        if expected != actual:
            where = "to the end" if index == len(baseline["checkpoints"]) else f"up to checkpoint {index + 1}"
            differences.append(f"the frames {where} differ in some channel (scene, hero, save, state, picture or sound); "
                               "`lom_routes.py compare` it against the build its baseline was recorded on to see what")
            break
    for expected, actual in zip(baseline["checkpoints"], run["checkpoints"]):
        if expected != actual:
            differences.append(f"checkpoint differs:\n    baseline {expected}\n    this run {actual}")
    if len(baseline["checkpoints"]) != len(run["checkpoints"]):
        differences.append(f"the run reached {len(run['checkpoints'])} checkpoints, the baseline {len(baseline['checkpoints'])}")
    if "changes" in baseline:
        changed = change_differences(baseline["changes"], run.get("changes", []))
        if changed:
            differences += changed
        elif baseline["checkpoints"] != run["checkpoints"]:
            differences.append("the change lists match, so what differs isn't in them; frames.txt in the run's record folder has the full state")
    for expected, actual in zip(baseline.get("states", []), run["states"]):
        if expected != actual:
            differences.append(f"game state differs at a checkpoint:\n    baseline {expected}\n    this run {actual}")
            break
    if baseline["notes"] != run["notes"]:
        differences.append("notes differ:\n    baseline " + "; ".join(baseline["notes"]) + "\n    this run " + "; ".join(run["notes"]))
    if baseline["end"] != run["end"]:
        differences.append(f"end differs: baseline {baseline['end']}, this run {run['end']}")
    return differences


def change_differences(baseline: list[dict], run: list[dict]) -> list[str]:
    """Where the change lists differ, checkpoint by checkpoint: the lines only one side has."""
    out = []
    for expected, actual in zip(baseline, run):
        only_baseline = [line for line in expected["changes"] if line not in actual["changes"]]
        only_run = [line for line in actual["changes"] if line not in expected["changes"]]
        if only_baseline or only_run:
            out.append(f"at checkpoint {expected['checkpoint']}:" + "".join(f"\n    baseline only: {line}" for line in only_baseline)
                       + "".join(f"\n    this run only: {line}" for line in only_run))
    return out


def check_written_save(cards: Path, built: bytes, layout: dict, expected: dict, prefix: str = lom_saves.SAVE_NAME_PREFIX) -> str | None:
    """For a route that saves: the problem with the save the game wrote to the card, or None. It has to be there
    (a card file, named with the game's save prefix, that isn't the one the route started from), valid, and have
    the fields the route expects."""
    written = [path for path in cards.rglob(f"{prefix}*") if path.read_bytes() != built]
    if not written:
        return "the game didn't write a save to the card"
    save = lom_saves.read(written[0].read_bytes(), layout)
    if not save["valid"]:
        return f"the save the game wrote doesn't read back: checksum {'ok' if save['checksum_ok'] else 'wrong'}, magic {'ok' if save['magic_ok'] else 'wrong'}"
    wrong = {name: (save["fields"].get(name), value) for name, value in expected.items() if save["fields"].get(name) != value}
    if wrong:
        return "the save the game wrote has " + ", ".join(f"{name} {got}, not {want}" for name, (got, want) in wrong.items())
    return None

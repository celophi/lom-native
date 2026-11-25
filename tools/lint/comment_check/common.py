from __future__ import annotations

import bisect
import re
from dataclasses import dataclass, field
from pathlib import Path

from .settings import CMAKE_NAMES, CMAKE_NAME_SUFFIXES, CMAKE_SUFFIXES, C_NAME_SUFFIXES, C_SUFFIXES, HASH_LINE_NAMES, HASH_LINE_SUFFIXES, LICENSE_MARKERS, PATCH_SUFFIXES, PYTHON_SUFFIXES, SHELL_SUFFIXES, TOOL_DIRECTIVE, XML_NAME_SUFFIXES, XML_SUFFIXES, YAML_NAMES, YAML_SUFFIXES


@dataclass
class Finding:
    path: str
    line: int
    start: int
    end: int
    kind: str
    text: str
    context: str = ""
    fixable: bool = True
    replacement: str = ""


@dataclass
class PatchLine:
    index: int
    marker: str
    text: str
    new_offset: int = -1


@dataclass
class Hunk:
    header_index: int
    old_start: int
    old_count: int
    new_start: int
    new_count: int
    tail: str
    lines: list = field(default_factory=list)


def line_starts(text):
    """Find where each line of a text starts.

    @param text The text.
    @return The offset of the first character of every line, in order.
    """
    starts = [0]
    for match in re.finditer("\n", text):
        starts.append(match.end())
    return starts


def line_of(starts, offset):
    """Turn a character offset into a line number.

    @param starts The text's line_starts.
    @param offset A character offset into the text.
    @return The 1-based line the offset is on.
    """
    return bisect.bisect_right(starts, offset)


def is_license(text):
    """Check whether a comment is one the rule keeps: a license or copyright
    notice, or a clang-format off or on.

    @param text The comment's text.
    @return True when it names a copyright or license, or is only that
            directive.
    """
    return any(marker in text for marker in LICENSE_MARKERS) or bool(TOOL_DIRECTIVE.match(text.strip()))


def language_of(path):
    """Work out which comment syntax a file uses.

    @param path The file's path, relative to the repository.
    @return "c", "python", "shell", "cmake", "yaml", "xml", "hash", "patch", or None for a file without comments to check.
    """
    name = Path(path).name
    suffix = Path(path).suffix
    if name in CMAKE_NAMES or suffix in CMAKE_SUFFIXES or name.endswith(CMAKE_NAME_SUFFIXES):
        return "cmake"
    if suffix in C_SUFFIXES or name.endswith(C_NAME_SUFFIXES):
        return "c"
    if suffix in PYTHON_SUFFIXES:
        return "python"
    if suffix in SHELL_SUFFIXES:
        return "shell"
    if suffix in YAML_SUFFIXES or name in YAML_NAMES:
        return "yaml"
    if suffix in XML_SUFFIXES or name.endswith(XML_NAME_SUFFIXES):
        return "xml"
    if suffix in HASH_LINE_SUFFIXES or name in HASH_LINE_NAMES:
        return "hash"
    if suffix in PATCH_SUFFIXES:
        return "patch"
    return None

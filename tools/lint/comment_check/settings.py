from __future__ import annotations

import json
import re
from pathlib import Path


DESCRIPTION = """Check the repository's files for comments. The only comment allowed is a
doc block on a function: /** ... */ or /// lines directly above a C or C++
function, or a docstring as the first statement of a Python def. License
notices, generated files and a patch's description don't count.

With no paths it checks every file git tracks. --fix takes the comments out,
and --report writes them all to a JSON file instead."""

REPOSITORY = Path(__file__).resolve().parents[3]

RULES = json.loads((Path(__file__).resolve().parent.parent / "comment_rules.json").read_text())

WALK_SKIPPED = set(RULES["skipped_folders"])

GENERATED_FILES = set(RULES["generated_files"])

LICENSE_MARKERS = tuple(RULES["license_markers"])

TOOL_DIRECTIVE = re.compile(r"^(//|/\*)\s*clang-format (off|on)\s*(\*/)?$")

C_SUFFIXES = {".c", ".h", ".cpp", ".cc", ".hpp", ".inc", ".m", ".mm", ".glsl", ".hlsl", ".metal"}

C_NAME_SUFFIXES = (".rc.in",)

PYTHON_SUFFIXES = {".py"}

SHELL_SUFFIXES = {".sh"}

CMAKE_NAMES = {"CMakeLists.txt"}

CMAKE_SUFFIXES = {".cmake"}

CMAKE_NAME_SUFFIXES = (".cmake.in",)

YAML_SUFFIXES = {".yml", ".yaml"}

YAML_NAMES = {".clang-format"}

XML_SUFFIXES = {".xml", ".qrc", ".ui"}

XML_NAME_SUFFIXES = (".plist.in",)

HASH_LINE_SUFFIXES = {".desktop"}

HASH_LINE_NAMES = {".gitignore", ".gitattributes", ".gitmodules"}

PATCH_SUFFIXES = {".patch"}

WHITESPACE = re.compile(r"\s*")

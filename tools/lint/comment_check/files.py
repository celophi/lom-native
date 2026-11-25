from __future__ import annotations

import os
import subprocess
from pathlib import Path

from .c_comments import scan_c
from .common import language_of
from .other_languages import scan_cmake, scan_hash_lines, scan_python, scan_shell, scan_xml, scan_yaml
from .patches import scan_patch
from .settings import GENERATED_FILES, REPOSITORY, WALK_SKIPPED


SCANNERS = {
    "c": scan_c,
    "python": scan_python,
    "shell": scan_shell,
    "cmake": scan_cmake,
    "yaml": scan_yaml,
    "xml": scan_xml,
    "hash": scan_hash_lines,
}


def scan_file(path):
    """Find the comments in one file that the rule doesn't allow.

    @param path The file's path, relative to the repository.
    @return The findings, empty for a file that's exempt or has no comments to check.
    """
    language = language_of(path)
    if language is None or path in GENERATED_FILES:
        return []
    text = (REPOSITORY / path).read_text(encoding="utf-8", errors="surrogateescape")
    if language == "patch":
        return scan_patch(path, text)
    return SCANNERS[language](path, text)


def tracked_files():
    """List the files git tracks, and the new ones it would.

    @return Their paths, relative to the repository: everything committed or
            staged, and every file not yet added that .gitignore doesn't leave
            out.
    """
    command = ["git", "-C", str(REPOSITORY), "ls-files", "-z", "--cached", "--others", "--exclude-standard"]
    try:
        output = subprocess.run(command, check=True, capture_output=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return walked_files()
    return [path for path in output.decode().split("\0") if path and (REPOSITORY / path).is_file()]


def walked_files():
    """List the repository's files without git, for a checkout git won't read.

    @return Every file's path, relative to the repository, leaving out git's
            own folder, build trees and the submodule.
    """
    paths = []
    for folder, folders, files in os.walk(REPOSITORY):
        folders[:] = sorted(name for name in folders if name not in WALK_SKIPPED)
        for name in sorted(files):
            paths.append((Path(folder) / name).relative_to(REPOSITORY).as_posix())
    return paths

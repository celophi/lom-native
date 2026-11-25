#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

DESCRIPTION = """Copy one PSY-Z file and apply our patches to the copy, in order.

The submodule is never changed. CMake calls this once per patched file, and
the result goes in the build tree, where the build compiles it instead of the
original.

git apply does the patching, with no fuzz, so every hunk's context has to match
the original exactly. The work happens in a temporary folder, and the output
only appears once every patch has applied. If anything fails, the old output is
deleted, so the build can never pick up a stale copy. CMake decides which file,
which patches and in what order.
"""

GIT_REPOSITORY_VARIABLES = ("GIT_DIR", "GIT_WORK_TREE", "GIT_COMMON_DIR", "GIT_INDEX_FILE")


def check_paths(source_root: Path, relative: Path, output: Path) -> None:
    """Refuse a source outside the tree, or an output inside it."""
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError("source must be a relative path within the source tree")
    if output.is_relative_to(source_root):
        raise ValueError("output must be outside the upstream source tree")


def isolated_git_environment(work: Path) -> dict[str, str]:
    """An environment where git only sees the temporary copy.

    TMPDIR might be inside a git checkout, and the caller might have git
    variables set. Either way, git apply should work on this copy and nothing
    else.
    """
    environment = os.environ.copy()
    for name in GIT_REPOSITORY_VARIABLES:
        environment.pop(name, None)
    environment["GIT_CEILING_DIRECTORIES"] = str(work.parent.resolve())
    return environment


def git_apply(git: str, work: Path, environment: dict[str, str], *arguments: str) -> bytes:
    """Run git apply in the work folder and return its output, or raise on failure."""
    result = subprocess.run(
        [git, "apply", "--no-index", *arguments],
        cwd=work,
        capture_output=True,
        check=False,
        env=environment,
    )
    if result.returncode:
        raise ValueError(result.stderr.decode(errors="replace").strip())
    return result.stdout


def check_patch_touches_only(git: str, work: Path, environment: dict[str, str], patch: Path, source: str, relative: Path) -> None:
    """Make sure a patch only changes the one file being prepared.

    This script handles one file at a time. A patch that touches other files is
    an error, instead of quietly dropping those changes.
    """
    stats = git_apply(git, work, environment, "--numstat", "-z", "--", str(patch)).split(b"\0")
    fields = stats[0].split(b"\t")
    touches_one_file = len(stats) == 2 and len(fields) == 3
    if (
        not touches_one_file
        or fields[2] != relative.as_posix().encode()
        or not fields[0].isdigit()
        or not fields[1].isdigit()
    ):
        raise ValueError(f"{patch}: expected only text changes to {source}")


def write_atomically(data: bytes, output: Path) -> None:
    """Write the result next to the output, then move it into place in one step."""
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as file:
        temporary = Path(file.name)
        try:
            file.write(data)
            file.close()
            os.replace(temporary, output)
        finally:
            temporary.unlink(missing_ok=True)


def prepare(source_root: Path, source: str, patches: list[Path], output: Path, git: str = "git") -> None:
    """Copy source_root/source, apply each patch in order, and write the result to output."""
    source_root = source_root.resolve()
    output = output.resolve()
    relative = Path(source)
    check_paths(source_root, relative, output)

    output.unlink(missing_ok=True)
    if not patches:
        raise ValueError("at least one native patch is required")

    with tempfile.TemporaryDirectory(prefix="lom-native-source-") as directory:
        work = Path(directory)
        staged = work / relative
        staged.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source_root / relative, staged)
        environment = isolated_git_environment(work)

        for patch in patches:
            patch = patch.resolve()
            check_patch_touches_only(git, work, environment, patch, source, relative)
            git_apply(git, work, environment, "--check", "--", str(patch))
            git_apply(git, work, environment, "--", str(patch))

        write_atomically(staged.read_bytes(), output)


def main() -> int:
    """Copy a file from a submodule with patches applied.

    @return 0. It stops with an error when a patch doesn't apply.
    """
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source_root", type=Path, help="the submodule the file comes from")
    parser.add_argument("--source", required=True, help="the file, relative to source_root")
    parser.add_argument("--patch", type=Path, action="append", required=True, help="a patch to apply; repeat in order")
    parser.add_argument("--output", type=Path, required=True, help="where the patched copy goes")
    parser.add_argument("--git", default="git", help="the git executable to use")
    args = parser.parse_args()

    try:
        prepare(args.source_root, args.source, args.patch, args.output, args.git)
    except (OSError, ValueError) as error:
        parser.exit(1, f"native source preparation failed: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

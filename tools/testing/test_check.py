#!/usr/bin/env python3

import argparse
import json
import re
import sys
from pathlib import Path

DESCRIPTION = """Checks every test is one kind of test, and says so where it lives and in its name.

There are five kinds, each in its own folder under tests/, with its own ending
on its files and on the program cmake builds from them, and its own ctest
label (ctest -L unit runs the unit tests):

  tests/unit/         *_test.c             lom-*-test            unit
      One piece of our code through its own interface. Nothing outside: no
      disc, no GPU, no --wrap. A provider's fake is fine.
  tests/providers/    *_provider_contract.c  lom-*-contract      contract
      A real provider against the real thing outside, so its fake can be
      trusted. provider-check makes sure every provider has one.
  tests/behavior/     *_specification.c    lom-*-specification   behavior
      A feature through the interface the game, a mod or a player uses, with
      what's outside us faked: by a provider's fake, or, where that outside
      thing has no provider yet, by a --wrap listed in the allow list. No
      internal headers and no including .c files.
  tests/integration/  *_integration.c      lom-*-integration     integration
      Our code against the real things outside: the disc, the GPU, files,
      processes, the audio device. Nothing faked.
  tests/component/    *_component.c        lom-*-component       component
      The old white-box tests, which call the game's internals and wrap our
      own functions. They break on every refactor, so there are no new ones:
      the allow list names each, and it only shrinks.

End-to-end tests are the routes in tests/routes, and static checks like this
one are scripts in tools/; both say their kind (e2e, static) where cmake adds
them. What tests share (fakes, fixtures, oracles, the route runner, test mods)
goes in tests/support.

A new test: pick its kind, put its file in that folder with that ending, build
it in cmake/native/tests.cmake as a program with that ending, and run it from
cmake/tests/registry.cmake. This check reads cmake's list of test programs
(test-targets.json in the build folder) and the allow list,
tools/testing/test_allow.json, which has to match exactly.
"""

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests"
ALLOW_LIST = ROOT / "tools" / "testing" / "test_allow.json"
KINDS = {
    "unit": {"folder": "unit", "file": "_test", "target": "-test"},
    "contract": {"folder": "providers", "file": "_contract", "target": "-contract"},
    "behavior": {"folder": "behavior", "file": "_specification", "target": "-specification"},
    "integration": {"folder": "integration", "file": "_integration", "target": "-integration"},
    "component": {"folder": "component", "file": "_component", "target": "-component"},
}
SHARED_FOLDERS = ("routes", "support")
SOURCE_ENDINGS = (".c", ".cpp")
WRAP = re.compile(r"--wrap=(\w+)")
WRAP_DEFINITION = re.compile(r"=__wrap_(\w+)")
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)


class Problems:
    def __init__(self) -> None:
        """Start with none."""
        self.count = 0

    def add(self, where: str, problem: str) -> None:
        """Print one problem, with where it is."""
        print(f"{where}: {problem}")
        self.count += 1


def kind_of_folder(path: Path) -> str | None:
    """The kind a file under tests/ is, from its folder, or None when it's in a shared folder."""
    folder = path.relative_to(TESTS).parts[0]
    return next((kind for kind, names in KINDS.items() if names["folder"] == folder), None)


def kind_of_target(target: str) -> str | None:
    """The kind a test program is, from the end of its name."""
    return next((kind for kind, names in KINDS.items() if target.endswith(names["target"])), None)


def a(kind: str) -> str:
    """A kind with "a" or "an" before it."""
    return ("an " if kind[0] in "aeiou" else "a ") + kind


def relative(path: Path) -> str:
    """A path as the repository writes it."""
    return path.relative_to(ROOT).as_posix()


def check_layout(problems: Problems) -> list[Path]:
    """Check tests/ holds only the kinds' folders and the shared ones, and that each test file has its kind's ending.
    Returns the test files."""
    folders = {names["folder"] for names in KINDS.values()}
    for entry in sorted(TESTS.iterdir()):
        if entry.name not in folders and entry.name not in SHARED_FOLDERS:
            problems.add(relative(entry), f"isn't a kind of test; tests go in {', '.join(sorted(folders))}, and what they share in support")
    files = []
    for kind, names in KINDS.items():
        folder = TESTS / names["folder"]
        if not folder.is_dir():
            continue
        for path in sorted(p for p in folder.rglob("*") if p.is_file()):
            if path.suffix not in SOURCE_ENDINGS:
                problems.add(relative(path), "only tests go in a kind's folder; put what tests share in tests/support")
                continue
            ending = "_provider_contract" if kind == "contract" else names["file"]
            if not path.stem.endswith(ending):
                problems.add(relative(path), f"is {a(kind)} test, so its name ends in \"{ending}{path.suffix}\"")
            files.append(path)
    return files


def wraps_of(target: dict) -> list[str]:
    """The functions a test program swaps out with the linker's --wrap."""
    found = set()
    for option in target.get("LINK_OPTIONS", []):
        found.update(WRAP.findall(option))
    for definition in target.get("COMPILE_DEFINITIONS", []):
        found.update(WRAP_DEFINITION.findall(definition))
    return sorted(found)


def test_sources(target: dict) -> list[Path]:
    """A test program's own test files: its sources under tests/ but not in a shared folder."""
    sources = []
    for source in target.get("SOURCES", []):
        if "$<" in source:
            continue
        path = Path(source) if Path(source).is_absolute() else ROOT / source
        path = path.resolve()
        if path.is_relative_to(TESTS) and kind_of_folder(path):
            sources.append(path)
    return sources


def check_targets(problems: Problems, manifest: dict, files: list[Path], allowed: dict) -> None:
    """Check each test program is built from tests of its own kind, is run, and fakes only what its kind may."""
    built = set()
    wrapped = {}
    for name, target in sorted(manifest.items()):
        kind = kind_of_target(name)
        sources = test_sources(target)
        built.update(sources)
        if not sources:
            problems.add(name, f"is named as {a(kind)} test, but has no source in tests/{KINDS[kind]['folder']}")
        for source in sources:
            if kind_of_folder(source) != kind:
                problems.add(name, f"is named as {a(kind)} test, but {relative(source)} is {a(kind_of_folder(source))} test; name the program for its kind")
        if not target.get("TESTS"):
            problems.add(name, "is built but nothing runs it; add it to cmake/tests/registry.cmake")
        labels = {label for run in target.get("TESTS", []) for label in run["labels"].split(",")}
        wraps = wraps_of(target)
        if kind == "unit" and labels & {"gpu", "disc"}:
            problems.add(name, "is a unit test, so it can't need the GPU or the disc; a test that does is an integration test")
        if kind in ("unit", "contract", "integration") and wraps:
            problems.add(name, f"swaps out {', '.join(wraps)} with --wrap, but {a(kind)} test fakes nothing that way")
        if kind == "behavior":
            for source in sources:
                wrapped[relative(source)] = sorted(set(wrapped.get(relative(source), [])) | set(wraps))
    for path in files:
        if path.resolve() not in built:
            problems.add(relative(path), "isn't built into any test program in cmake, so nothing runs it")
    listed = allowed.get("behavior_wraps", {})
    for source, wraps in sorted(wrapped.items()):
        if not wraps:
            continue
        if source not in listed:
            problems.add(source, f"fakes {', '.join(wraps)} with --wrap; fake through a provider, or list it in {ALLOW_LIST.name} with why")
        elif sorted(listed[source]["wraps"]) != wraps:
            problems.add(source, f"wraps {', '.join(wraps)}, but {ALLOW_LIST.name} lists {', '.join(sorted(listed[source]['wraps']))}")
    for source in sorted(listed):
        if not wrapped.get(source):
            problems.add(source, f"no longer fakes anything with --wrap; take it out of {ALLOW_LIST.name}")


def check_sources(problems: Problems, files: list[Path], allowed: dict) -> None:
    """Check what each test includes is right for its kind, and that the component tests are only the listed ones."""
    for path in files:
        kind = kind_of_folder(path)
        includes = INCLUDE.findall(path.read_text(errors="replace"))
        if kind in ("contract", "integration"):
            for include in includes:
                if include.endswith("_provider_fake.h"):
                    problems.add(relative(path), f"is {a(kind)} test, so it uses the real provider, not {include}")
        if kind == "behavior":
            for include in includes:
                if "/internal/" in include or include.endswith("_internal.h") or include.endswith(SOURCE_ENDINGS):
                    problems.add(relative(path), f"is a behavioral test, so it goes through interfaces, not {include}")
    components = {relative(path) for path in files if kind_of_folder(path) == "component"}
    listed = allowed.get("component", {})
    for source in sorted(components - set(listed)):
        problems.add(source, "is a new component test; write it as a behavioral or unit test instead")
    for source in sorted(set(listed) - components):
        problems.add(source, f"isn't a component test any more; take it out of {ALLOW_LIST.name}")


def main() -> int:
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("manifest", type=Path, help="test-targets.json, which cmake writes in the build folder")
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text())
    allowed = json.loads(ALLOW_LIST.read_text())
    problems = Problems()
    files = check_layout(problems)
    check_targets(problems, manifest, files, allowed)
    check_sources(problems, files, allowed)
    if problems.count:
        print(f"\n{problems.count} problem{'s' if problems.count != 1 else ''}; run this with --help for the kinds of test")
        return 1
    counts = {kind: sum(1 for path in files if kind_of_folder(path) == kind) for kind in KINDS}
    print("tests: " + ", ".join(f"{count} {kind}" for kind, count in counts.items()))
    return 0


if __name__ == "__main__":
    sys.exit(main())

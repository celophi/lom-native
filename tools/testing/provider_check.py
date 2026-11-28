#!/usr/bin/env python3

import argparse
import json
import re
import sys
from pathlib import Path

DESCRIPTION = """Checks that code reaches the outside world only through its providers.

A provider is the one thin place a part of the port reaches something outside
it, like the disk for the Remaster's files. Everything above it is the port's
own code, which tests run for real with a provider of their own (a fake that
serves files from memory, say). That only works while nothing goes around the
provider, so this check finds the calls in a provider's area that belong to
the provider alone, and compares them with an allow list,
tools/testing/provider_allow.json, of the ones still to move. The list has to
match exactly: a new call fails, and so does one that's gone but still listed,
so it only ever shrinks. Each entry says why it's there.

A new provider goes in PROVIDERS: its name, its own files, its contract test,
the fake other tests use in its place, the files it covers and the calls only
it may make. A provider says so in its name: its name and its files end in
"provider" (remaster_files_provider.c), its contract test, in tests/providers,
in "_provider_contract.c", and its fake in "_provider_fake.h", so anyone
reading the code knows it's the seam tests replace. Every provider has both,
and its contract test is built, so CI runs it.
"""

ROOT = Path(__file__).resolve().parents[2]
ALLOW_LIST = ROOT / "tools" / "testing" / "provider_allow.json"
TESTS_CMAKE = ROOT / "cmake" / "native" / "tests.cmake"
FILE_CALLS = ("fopen", "freopen", "_wfopen", "open", "opendir", "stat", "lstat", "_wstat", "access", "CreateFileA", "CreateFileW")
PROVIDERS = [
    {
        "name": "remaster files provider",
        "provider": ["src/remaster/remaster_files_provider.c"],
        "contract_test": "tests/providers/remaster_files_provider_contract.c",
        "fake": "tests/support/platform/remaster_files_provider_fake.h",
        "covers": ["src/remaster/*.c"],
        "calls": FILE_CALLS,
    },
]
FUNCTION_START = re.compile(r"^(?:static\s+|inline\s+|const\s+|unsigned\s+|struct\s+)*[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\([^;]*$")


def code_lines(text: str) -> list[str]:
    """The file's lines with comments, strings and character literals blanked out, so neither braces nor call names
    in them count."""
    out, i, n = [], 0, len(text)
    buffer = []
    while i < n:
        c = text[i]
        if text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            buffer.append("".join(ch if ch == "\n" else " " for ch in text[i:end]))
            i = end
        elif text.startswith("//", i):
            end = text.find("\n", i)
            end = n if end < 0 else end
            buffer.append(" " * (end - i))
            i = end
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            buffer.append(c + " " * (min(j, n) - i - 1) + (c if j < n else ""))
            i = j + 1
        else:
            buffer.append(c)
            i += 1
    out = "".join(buffer).split("\n")
    return out


def calls_in(path: Path, names: tuple[str, ...]) -> dict[str, list[str]]:
    """The calls to some functions in a C file, by the function they're in."""
    pattern = re.compile(r"(?<![\w.>])(" + "|".join(map(re.escape, names)) + r")\s*\(")
    found: dict[str, list[str]] = {}
    depth, current = 0, None
    for line in code_lines(path.read_text(errors="replace")):
        if depth == 0:
            match = FUNCTION_START.match(line)
            if match:
                current = match.group(1)
        if depth > 0 and current:
            for call in pattern.findall(line):
                found.setdefault(current, [])
                if call not in found[current]:
                    found[current].append(call)
        depth += line.count("{") - line.count("}")
        if depth == 0 and "}" in line:
            current = None
    return found


def findings() -> dict[str, dict[str, list[str]]]:
    """Every provider's calls made outside it, as {provider: {"file:function": [calls]}}."""
    result = {}
    for provider in PROVIDERS:
        own = {ROOT / path for path in provider["provider"]}
        calls = {}
        for pattern in provider["covers"]:
            for path in sorted(ROOT.glob(pattern)):
                if path in own:
                    continue
                for function, names in calls_in(path, tuple(provider["calls"])).items():
                    calls[f"{path.relative_to(ROOT).as_posix()}:{function}"] = sorted(names)
        result[provider["name"]] = calls
    return result


def naming_problems() -> bool:
    """Check every provider is named as one, its name and its own files ending in "provider", that its files are
    there, and that it has a contract test, which is built, and a fake. Prints each problem; returns whether there were
    any."""
    failed = False
    for provider in PROVIDERS:
        if not provider["name"].endswith(" provider"):
            print(f"{provider['name']}: a provider's name ends in \"provider\"")
            failed = True
        for path in provider["provider"]:
            if not Path(path).stem.endswith("_provider"):
                print(f"{provider['name']}: {path} is the provider's own file, so its name ends in \"_provider\"")
                failed = True
            if not (ROOT / path).exists():
                print(f"{provider['name']}: {path} isn't there")
                failed = True
        for key, ending, what in (("contract_test", "_provider_contract.c", "contract test"), ("fake", "_provider_fake.h", "fake")):
            path = provider.get(key)
            if not path:
                print(f"{provider['name']}: has no {what}; every provider has one")
                failed = True
            elif not path.endswith(ending) or not (ROOT / path).exists() or (key == "contract_test" and not path.startswith("tests/providers/")):
                where = " in tests/providers" if key == "contract_test" else ""
                print(f"{provider['name']}: its {what} {path} isn't there{where}, or its name doesn't end in \"{ending}\"")
                failed = True
        contract_test = provider.get("contract_test")
        if contract_test and contract_test not in TESTS_CMAKE.read_text():
            print(f"{provider['name']}: its contract test {contract_test} isn't built in {TESTS_CMAKE.relative_to(ROOT).as_posix()}, so nothing runs it")
            failed = True
    return failed


def main() -> int:
    argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter).parse_args()
    allowed = json.loads(ALLOW_LIST.read_text())["providers"]
    failed = naming_problems()
    for name, calls in findings().items():
        listed = allowed.get(name, {})
        for where, names in calls.items():
            if where not in listed:
                print(f"{name}: {where} calls {', '.join(names)} itself; go through the provider, or list it in {ALLOW_LIST.name} with why")
                failed = True
            elif sorted(listed[where]["calls"]) != names:
                print(f"{name}: {where} calls {', '.join(names)}, but {ALLOW_LIST.name} lists {', '.join(sorted(listed[where]['calls']))}")
                failed = True
        for where in listed:
            if where not in calls:
                print(f"{name}: {where} no longer goes around the provider; take it out of {ALLOW_LIST.name}")
                failed = True
    if failed:
        return 1
    total = sum(len(calls) for calls in findings().values())
    print(f"providers: {len(PROVIDERS)} checked, {total} known way{'s' if total != 1 else ''} around them still listed")
    return 0


if __name__ == "__main__":
    sys.exit(main())

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
it may make. Two providers can reach the same edge for different things (the
launcher's update feed and its downloads both use the network), so each may
name the other's files as "shares_edge_with".

A provider says so in its name: its name and its files end in "provider"
(remaster_files_provider.c), its contract test, in tests/providers, in
"_provider_contract.c" (or .cpp), and its fake in "_provider_fake.h" (or a
gmock one in "_provider_mock.h"), so anyone reading the code knows it's the
seam tests replace. Every provider has both,
and its contract test is built, so CI runs it.
"""

ROOT = Path(__file__).resolve().parents[2]
ALLOW_LIST = ROOT / "tools" / "testing" / "provider_allow.json"
TESTS_CMAKE = (ROOT / "cmake" / "native" / "tests.cmake", ROOT / "launcher" / "tests" / "CMakeLists.txt")
FILE_CALLS = ("fopen", "freopen", "_wfopen", "open", "opendir", "stat", "lstat", "_wstat", "access", "CreateFileA", "CreateFileW")
HD_RENDERER_CALLS = ("lom_hd_texture_create", "lom_hd_texture_upload", "lom_hd_texture_mipmaps", "lom_hd_texture_release", "lom_hd_quad")
PROVIDERS = [
    {
        "name": "remaster files provider",
        "provider": ["src/remaster/remaster_files_provider.c"],
        "contract_test": "tests/providers/remaster_files_provider_contract.c",
        "fake": "tests/support/platform/remaster_files_provider_fake.h",
        "covers": ["src/remaster/*.c"],
        "calls": FILE_CALLS,
    },
    {
        "name": "disc files provider",
        "provider": ["src/cd/disc_files_provider.c"],
        "contract_test": "tests/providers/disc_files_provider_contract.c",
        "fake": "tests/support/cd/disc_files_provider_fake.h",
        "covers": ["src/cd/*.c"],
        "calls": FILE_CALLS,
    },
    {
        "name": "hd renderer provider",
        "provider": ["src/gpu/hd_renderer_provider.c"],
        "contract_test": "tests/providers/hd_renderer_provider_contract.c",
        "fake": "tests/support/gpu/hd_renderer_provider_fake.h",
        "covers": ["src/remaster/*.c", "src/movie/*.c", "src/gpu/*.c", "src/ui/*.cpp"],
        "calls": HD_RENDERER_CALLS,
    },
    {
        "name": "launcher clock provider",
        "provider": ["launcher/core/src/common/clock_provider.cpp"],
        "contract_test": "launcher/tests/providers/clock_provider_contract.cpp",
        "fake": "launcher/tests/behavior/common/mocks/clock_provider_mock.h",
        "covers": ["launcher/core/src/**/*.cpp", "launcher/app/**/*.cpp"],
        "calls": ("re:system_clock::now",),
    },
    {
        "name": "launcher random provider",
        "provider": ["launcher/core/src/common/random_provider.cpp"],
        "contract_test": "launcher/tests/providers/random_provider_contract.cpp",
        "fake": "launcher/tests/behavior/common/mocks/random_provider_mock.h",
        "covers": ["launcher/core/src/**/*.cpp", "launcher/app/**/*.cpp"],
        "calls": (r"re:\brandom_device\b",),
    },
    {
        "name": "launcher desktop provider",
        "provider": ["launcher/app/services/system_desktop_provider.cpp"],
        "contract_test": "launcher/tests/providers/desktop_provider_contract.cpp",
        "fake": "launcher/tests/behavior/common/mocks/desktop_provider_mock.h",
        "covers": ["launcher/core/src/**/*.cpp", "launcher/app/**/*.cpp"],
        "calls": ("QDesktopServices::openUrl",),
    },
    {
        "name": "launcher file dialogs provider",
        "provider": ["launcher/app/services/native_file_dialogs_provider.cpp"],
        "contract_test": "launcher/tests/providers/file_dialogs_provider_contract.cpp",
        "fake": "launcher/tests/behavior/common/mocks/file_dialogs_provider_mock.h",
        "covers": ["launcher/core/src/**/*.cpp", "launcher/app/**/*.cpp"],
        "calls": ("QFileDialog::getOpenFileName", "QFileDialog::getOpenFileNames", "QFileDialog::getExistingDirectory", "QFileDialog::getSaveFileName"),
    },
    {
        "name": "launcher update feed provider",
        "provider": ["launcher/app/services/github_update_feed_provider.cpp"],
        "contract_test": "launcher/tests/providers/update_feed_provider_contract.cpp",
        "fake": "launcher/tests/behavior/common/mocks/update_feed_provider_mock.h",
        "covers": ["launcher/core/src/**/*.cpp", "launcher/app/**/*.cpp"],
        "calls": (r"re:\bQNetworkAccessManager\b",),
        "shares_edge_with": ["launcher/app/services/qt_download_provider.cpp"],
    },
    {
        "name": "launcher download provider",
        "provider": ["launcher/app/services/qt_download_provider.cpp"],
        "contract_test": "launcher/tests/providers/download_provider_contract.cpp",
        "fake": "launcher/tests/behavior/common/mocks/download_provider_mock.h",
        "covers": ["launcher/core/src/**/*.cpp", "launcher/app/**/*.cpp"],
        "calls": (r"re:\bQNetworkAccessManager\b",),
        "shares_edge_with": ["launcher/app/services/github_update_feed_provider.cpp"],
    },
    {
        "name": "launcher process runner provider",
        "provider": ["launcher/app/services/qt_process_runner_provider.cpp"],
        "contract_test": "launcher/tests/providers/process_runner_provider_contract.cpp",
        "fake": "launcher/tests/behavior/common/mocks/process_runner_provider_mock.h",
        "covers": ["launcher/core/src/**/*.cpp", "launcher/app/**/*.cpp"],
        "calls": (r"re:\bQProcess\b",),
        "shares_edge_with": ["launcher/app/services/qt_tools_provider.cpp"],
    },
    {
        "name": "launcher tools provider",
        "provider": ["launcher/app/services/qt_tools_provider.cpp"],
        "contract_test": "launcher/tests/providers/tools_provider_contract.cpp",
        "fake": "launcher/tests/behavior/common/mocks/tools_provider_mock.h",
        "covers": ["launcher/core/src/**/*.cpp", "launcher/app/**/*.cpp"],
        "calls": (r"re:\bQProcess\b",),
        "shares_edge_with": ["launcher/app/services/qt_process_runner_provider.cpp"],
    },
    {
        "name": "launcher files provider",
        "provider": ["launcher/core/src/common/files_provider.cpp"],
        "contract_test": "launcher/tests/providers/files_provider_contract.cpp",
        "fake": "launcher/tests/support/files_provider_fake.h",
        "covers": ["launcher/core/src/**/*.cpp", "launcher/app/**/*.cpp"],
        "calls": (r"re:std::filesystem::(create_directories|exists|remove_all|rename|remove|is_directory|file_size|last_write_time|is_regular_file|copy_file|copy|space|weakly_canonical|equivalent|permissions|status)\s*\(",
                  r"re:\bstd::[io]?fstream\b", r"re:\bdirectory_iterator\b", r"re:\bQFile\b", r"re:\bQFileInfo\b", r"re:\bQTemporaryFile\b", r"re:\bQSaveFile\b"),
    },
    {
        "name": "launcher file lock provider",
        "provider": ["launcher/app/services/qt_file_lock_provider.cpp"],
        "contract_test": "launcher/tests/providers/file_lock_provider_contract.cpp",
        "fake": "launcher/tests/behavior/common/mocks/file_lock_provider_mock.h",
        "covers": ["launcher/core/src/**/*.cpp", "launcher/app/**/*.cpp"],
        "calls": (r"re:\bQLockFile\b",),
    },
    {
        "name": "mod files provider",
        "provider": ["src/mods/mod_files_provider.cpp"],
        "contract_test": "tests/providers/mod_files_provider_contract.cpp",
        "fake": "tests/support/mods/mod_files_provider_fake.h",
        "covers": ["src/mods/*.cpp", "src/mods/*.c"],
        "calls": FILE_CALLS + (r"re:std::filesystem::(create_directories|exists|remove_all|rename|remove|is_directory|file_size|is_regular_file|copy_file|copy)\s*\(",
                               r"re:\bstd::[io]?fstream\b", r"re:\bdirectory_iterator\b"),
    },
]
FUNCTION_START = re.compile(r"^(?!\s*(?:if|for|while|switch|return|else|do|case)\b)[\w:<>,\*&\s~]*?\b([A-Za-z_~][\w:~]*)\s*\([^;]*$")
TYPE_START = re.compile(r"^\s*(?:class|struct)\s+(\w+)[^;]*$")
NAMESPACE_START = re.compile(r"^\s*namespace\b[^;]*\{")


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


def call_patterns(calls: tuple[str, ...]) -> list[re.Pattern]:
    """Patterns for the calls only a provider makes: a name is a call of that function ("fopen", or a qualified one,
    "QDesktopServices::openUrl"), and "re:" starts a pattern of its own, for what isn't a call (a type, say)."""
    return [re.compile(call[3:]) if call.startswith("re:") else re.compile(r"(?<![\w.>])" + re.escape(call) + r"\s*\(") for call in calls]


def calls_in(path: Path, calls: tuple[str, ...]) -> dict[str, list[str]]:
    """The calls a provider alone makes, in a C or C++ file, by the function they're in (or the type, for one inside a
    class's body). A namespace's braces don't count, so its functions are found as a file's are."""
    patterns = call_patterns(calls)
    found: dict[str, list[str]] = {}
    depth, current = 0, None
    for line in code_lines(path.read_text(errors="replace")):
        opens, closes = line.count("{"), line.count("}")
        if NAMESPACE_START.match(line):
            opens -= 1
        if depth == 0:
            match = TYPE_START.match(line) or FUNCTION_START.match(line)
            if match:
                current = match.group(1)
        for pattern, call in zip(patterns, calls):
            if not line.lstrip().startswith("#") and pattern.search(line):
                where = current or "(file)"
                found.setdefault(where, [])
                if call not in found[where]:
                    found[where].append(call)
        depth = max(0, depth + opens - closes)
        if depth == 0 and closes:
            current = None
    return found


def findings() -> dict[str, dict[str, list[str]]]:
    """Every provider's calls made outside it, as {provider: {"file:function": [calls]}}."""
    result = {}
    for provider in PROVIDERS:
        own = {ROOT / path for path in provider["provider"] + provider.get("shares_edge_with", [])}
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
        for key, endings, what in (("contract_test", ("_provider_contract.c", "_provider_contract.cpp"), "contract test"),
                                   ("fake", ("_provider_fake.h", "_provider_mock.h"), "fake")):
            path = provider.get(key)
            if not path:
                print(f"{provider['name']}: has no {what}; every provider has one")
                failed = True
            elif not path.endswith(endings) or not (ROOT / path).exists() or (key == "contract_test" and "tests/providers/" not in path):
                where = " in a tests/providers folder" if key == "contract_test" else ""
                print(f"{provider['name']}: its {what} {path} isn't there{where}, or its name doesn't end in {' or '.join(endings)}")
                failed = True
        contract_test = provider.get("contract_test")
        builds = "".join(cmake.read_text() for cmake in TESTS_CMAKE)
        if contract_test and contract_test not in builds and contract_test.removeprefix("launcher/tests/") not in builds:
            print(f"{provider['name']}: its contract test {contract_test} isn't built, so nothing runs it")
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

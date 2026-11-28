#!/usr/bin/env python3

import argparse
import json
import re
import sys
from pathlib import Path

from provider_check import code_lines

DESCRIPTION = """Checks every behavioral specification is written as given, when, then.

A specification (SPECIFICATION(name, "sentence") in tests/behavior) is any
number of givens, then a single when, then everything after it is a then:

  SPECIFICATION(draws_the_japanese_logo, "Title backdrop should ...")
  {
      s_text = LOM_REMASTER_TEXT_JAPANESE;     given, none or more

      when_the_title_draws_its_backdrop();      when, exactly one

      CHECK(s_renderer_fake.quads[1].colour == JAPANESE_LOGO);   then
  }

The when is a call to the file's when_ function, the action every
specification in the file takes on the system, so they all call the same one.
Nothing before it checks anything, since a check is a then, and after it there
is at least one check. The specification's doc block says the same in words,
in the same order: "Given ...; when ...; then ...", the given part left out
when there is none.

Every file in tests/behavior is written this way, but for the older ones in
tools/testing/test_allow.json's behavior_not_given_when_then, a list that has
to match exactly and only shrinks.
"""

ROOT = Path(__file__).resolve().parents[2]
ALLOW_LIST = ROOT / "tools" / "testing" / "test_allow.json"
SPECIFICATIONS = ("tests/behavior/**/*_specification.c", "tests/behavior/**/*_specification.cpp")
SPECIFICATION = re.compile(r"^SPECIFICATION\s*\(\s*(\w+)", re.MULTILINE)
WHEN = re.compile(r"\b(when_\w+)\s*\(")
CHECK = re.compile(r"\bCHECK\w*\s*\(")
DOC_ORDER = re.compile(r"^(?:given\b.*?;\s*)?when\b.*?;\s*then\b", re.IGNORECASE | re.DOTALL)


def statements(body: str) -> list[tuple[int, str]]:
    """Split a function's body, with its comments and strings blanked, into its statements, a block counting as one.
    Returns each with where it starts in the body."""
    found, start, parens, braces = [], 0, 0, 0
    for at, character in enumerate(body):
        if character == "(":
            parens += 1
        elif character == ")":
            parens -= 1
        elif character == "{":
            braces += 1
        elif character == "}":
            braces -= 1
            if braces == 0 and parens == 0:
                found.append((start, body[start:at + 1]))
                start = at + 1
        elif character == ";" and parens == 0 and braces == 0:
            found.append((start, body[start:at + 1]))
            start = at + 1
    return [(offset + len(text) - len(text.lstrip()), text.strip()) for offset, text in found if text.strip()]


def body_of(code: str, start: int) -> tuple[int, int] | None:
    """Find the braces of the function that starts at an offset. Returns where its body starts and ends."""
    opening = code.find("{", start)
    if opening < 0:
        return None
    depth = 0
    for at in range(opening, len(code)):
        if code[at] == "{":
            depth += 1
        elif code[at] == "}":
            depth -= 1
            if depth == 0:
                return opening + 1, at
    return None


def doc_text(source: str, start: int) -> str | None:
    """The words of the doc block right before an offset, without its stars and tags' names."""
    before = source[:start].rstrip()
    if not before.endswith("*/"):
        return None
    opening = before.rfind("/**")
    if opening < 0:
        return None
    lines = [re.sub(r"^\s*\*\s?", "", line) for line in before[opening + 3:-2].splitlines()]
    return re.sub(r"@\w+\s*", "", " ".join(line.strip() for line in lines)).strip()


def check_file(path: Path, problems: list[str]) -> int:
    """Check each specification in a file. Adds what's wrong to problems; returns how many specifications it has."""
    source = path.read_text(errors="replace")
    code = "\n".join(code_lines(source))
    relative = path.relative_to(ROOT).as_posix()
    whens = set()
    count = 0
    for match in SPECIFICATION.finditer(code):
        count += 1
        name = match.group(1)
        line = code.count("\n", 0, match.start()) + 1
        where = f"{relative}:{line}: {name}"
        span = body_of(code, match.end())
        if not span:
            problems.append(f"{where}: has no body")
            continue
        body = code[span[0]:span[1]]
        parts = statements(body)
        when_at = [i for i, (_, text) in enumerate(parts) if WHEN.search(text)]
        if len(when_at) != 1:
            problems.append(f"{where}: has {len(when_at)} whens; a specification has exactly one, a call to the file's when_ function")
            continue
        when = when_at[0]
        whens.add(WHEN.search(parts[when][1]).group(1))
        if WHEN.search(parts[when][1]) and not re.fullmatch(r"when_\w+\s*\(\s*\)\s*;", parts[when][1]):
            problems.append(f"{where}: its when does more than call the when_ function; put the rest in the givens or the thens")
        for _, text in parts[:when]:
            if CHECK.search(text):
                problems.append(f"{where}: checks something before its when; a check is a then, and goes after it")
        if not any(CHECK.search(text) for _, text in parts[when + 1:]):
            problems.append(f"{where}: has no then; check at least one thing after the when")
        doc = doc_text(source, match.start())
        if doc is None:
            problems.append(f"{where}: has no doc block; say its given, when and then in one")
        elif not DOC_ORDER.search(doc):
            problems.append(f"{where}: its doc block doesn't read \"Given ...; when ...; then ...\" in that order")
    if len(whens) > 1:
        problems.append(f"{relative}: its specifications call {', '.join(sorted(whens))}; every one in a file takes the same action, so they call the same when")
    return count


def main() -> int:
    argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter).parse_args()
    problems: list[str] = []
    files = sorted({path for pattern in SPECIFICATIONS for path in ROOT.glob(pattern)})
    unwritten = set(json.loads(ALLOW_LIST.read_text()).get("behavior_not_given_when_then", {}))
    total = 0
    for path in files:
        relative = path.relative_to(ROOT).as_posix()
        count = check_file(path, problems)
        total += count
        if count == 0 and relative not in unwritten:
            problems.append(f"{relative}: has no SPECIFICATION; write its tests as given, when, then (tests/support/behavior/specification.h)")
        elif count and relative in unwritten:
            problems.append(f"{relative}: is written as specifications now; take it out of {ALLOW_LIST.name}")
    for relative in sorted(unwritten - {path.relative_to(ROOT).as_posix() for path in files}):
        problems.append(f"{relative}: isn't there any more; take it out of {ALLOW_LIST.name}")
    for problem in problems:
        print(problem)
    if problems:
        print(f"\n{len(problems)} problem{'s' if len(problems) != 1 else ''}; run this with --help for how a specification is written")
        return 1
    print(f"specifications: {total} in {len(files) - len(unwritten)} files, each given, when, then; {len(unwritten)} older files still to rewrite")
    return 0


if __name__ == "__main__":
    sys.exit(main())

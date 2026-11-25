#!/usr/bin/env python3

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

RELEASE = re.compile(r'<release version="(\d+)\.(\d+)\.(\d+)"')
TAG = re.compile(r"v(\d+)\.(\d+)\.(\d+)")


def main() -> int:
    """Add a release entry to the AppStream metainfo for each version tag it doesn't list yet.

    @return 0, 1 when the file has no <releases>, or 2 when it's run wrong.
    """
    if len(sys.argv) != 2:
        print("usage: add-releases.py <metainfo.xml>", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    text = path.read_text()
    listed = [tuple(map(int, match.groups())) for match in RELEASE.finditer(text)]
    newest = max(listed, default=(0, 0, 0))

    tags = subprocess.run(["git", "for-each-ref", "refs/tags/v*", "--format=%(refname:strip=2) %(creatordate:short)"],
                          capture_output=True, text=True, check=True).stdout
    new = []
    for line in tags.splitlines():
        name, date = line.split()
        match = TAG.fullmatch(name)
        if match and tuple(map(int, match.groups())) > newest:
            new.append((tuple(map(int, match.groups())), date))
    new.sort(reverse=True)

    opening = re.search(r"^( *)<releases>\n", text, re.MULTILINE)
    if not opening:
        print(f"{path}: no <releases> to add to", file=sys.stderr)
        return 1
    indent = opening.group(1) + "  "
    entries = "".join(f'{indent}<release version="{".".join(map(str, version))}" date="{date}"/>\n' for version, date in new)
    path.write_text(text[: opening.end()] + entries + text[opening.end() :])
    for version, date in new:
        print(f"added {'.'.join(map(str, version))} ({date})")
    if not new:
        print("no tags newer than " + ".".join(map(str, newest)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

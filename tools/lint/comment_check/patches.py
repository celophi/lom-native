from __future__ import annotations

import re

from .c_comments import find_c_comments, scan_c
from .common import Finding, Hunk, PatchLine, language_of, line_of, line_starts


HUNK_HEADER = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@(.*)$")


def parse_patch(text):
    """Split a patch into its description, its files' headers and its hunks.

    @param text The patch.
    @return The patch's lines, and a list of (target path, hunks) per file.
    """
    lines = text.split("\n")
    files = []
    target = None
    hunk = None
    in_description = True
    for index, line in enumerate(lines):
        if line.startswith("--- ") and (hunk is None or hunk_done(hunk)):
            in_description = False
            hunk = None
            continue
        if in_description:
            continue
        if line.startswith("+++ ") and hunk is None:
            target = line[4:].split("\t")[0]
            target = target[2:] if target.startswith(("a/", "b/")) else target
            files.append((target, []))
            continue
        header = HUNK_HEADER.match(line)
        if header and (hunk is None or hunk_done(hunk)):
            hunk = Hunk(index, int(header.group(1)), int(header.group(2) or 1), int(header.group(3)),
                        int(header.group(4) or 1), header.group(5))
            files[-1][1].append(hunk)
            continue
        if hunk is not None and not hunk_done(hunk) and line[:1] in (" ", "+", "-", "\\"):
            hunk.lines.append(PatchLine(index, line[:1], line[1:]))
        elif hunk is not None and not hunk_done(hunk) and line == "":
            hunk.lines.append(PatchLine(index, " ", ""))
    return lines, files


def hunk_done(hunk):
    """Check whether a hunk has all the lines its header promises.

    @param hunk The hunk.
    @return True once it has its old and new line counts.
    """
    old = sum(1 for line in hunk.lines if line.marker in " -")
    new = sum(1 for line in hunk.lines if line.marker in " +")
    return old >= hunk.old_count and new >= hunk.new_count


def scan_patch(path, text):
    """Find the comments a patch adds to C-like files.

    Only comments entirely on the patch's added lines count, and not those the
    hunk's old side already has, which an added line only carries over. The
    rest belong to the file it changes, and its description isn't code.

    @param path The patch's path.
    @param text The patch.
    @return The findings, with offsets into the patch.
    """
    lines, files = parse_patch(text)
    starts = line_starts(text)
    findings = []
    for target, hunks in files:
        if language_of(target) != "c":
            continue
        for hunk in hunks:
            new_side = []
            position = 0
            for patch_line in hunk.lines:
                if patch_line.marker in " +":
                    patch_line.new_offset = position
                    new_side.append(patch_line.text)
                    position += len(patch_line.text) + 1
            source = "\n".join(new_side)
            old_side = "\n".join(line.text for line in hunk.lines if line.marker in " -")
            old_comments = {old_side[start:end] for start, end in find_c_comments(old_side)[0]}
            new_lines = [line for line in hunk.lines if line.marker in " +"]
            new_starts = line_starts(source)
            for finding in scan_c(target, source):
                if finding.text in old_comments:
                    continue
                first = line_of(new_starts, finding.start) - 1
                last = line_of(new_starts, max(finding.start, finding.end - 1)) - 1
                if not all(line.marker == "+" for line in new_lines[first:last + 1]):
                    continue
                first_line = new_lines[first]
                last_line = new_lines[last]
                start = starts[first_line.index] + 1 + finding.start - first_line.new_offset
                end = starts[last_line.index] + 1 + finding.end - last_line.new_offset
                findings.append(Finding(path, first_line.index + 1, start, end, finding.kind, finding.text, finding.context))
    return findings


def fix_patch_counts(text):
    """Make every hunk header of a patch match its lines again.

    @param text The patch, after lines came out of it.
    @return The patch with each header's counts, and the new-side starts of the
            hunks after it, set from its lines.
    """
    lines = text.split("\n")
    output = []
    shift = 0
    index = 0
    while index < len(lines):
        line = lines[index]
        if line.startswith("--- "):
            shift = 0
        header = HUNK_HEADER.match(line)
        if not header:
            output.append(line)
            index += 1
            continue
        old_count = int(header.group(2) or 1)
        body = []
        index += 1
        old_seen = 0
        while index < len(lines) and (old_seen < old_count or (lines[index][:1] == "+")):
            body_line = lines[index]
            if body_line[:1] not in (" ", "+", "-", "\\") and body_line != "":
                break
            if HUNK_HEADER.match(body_line) or body_line.startswith("--- "):
                break
            if body_line[:1] in (" ", "-") or body_line == "":
                old_seen += 1
            body.append(body_line)
            index += 1
        new_count = sum(1 for body_line in body if body_line[:1] in (" ", "+") or body_line == "")
        declared_new = int(header.group(4) or 1)
        new_start = int(header.group(3)) + shift
        shift += new_count - declared_new
        output.append(f"@@ -{header.group(1)},{old_count} +{new_start},{new_count} @@{header.group(5)}")
        output.extend(body)
    return "\n".join(output)


def patch_removal_range(text, start, end):
    """Work out what to delete to take a comment out of a patch's added lines.

    @param text The patch.
    @param start Where the comment starts.
    @param end Where it ends.
    @return (start, end, whole lines) of the text to delete. A comment alone on
            its added lines takes those lines, '+' and all.
    """
    line_start = text.rfind("\n", 0, start) + 1
    line_end = text.find("\n", end)
    line_end = len(text) if line_end < 0 else line_end
    before = text[line_start + 1:start]
    after = text[end:line_end]
    if not before.strip() and not after.strip():
        return line_start, min(line_end + 1, len(text)), True
    if before.strip() and not after.strip():
        return start - (len(before) - len(before.rstrip())), end, False
    return start, end + (len(after) - len(after.lstrip())), False

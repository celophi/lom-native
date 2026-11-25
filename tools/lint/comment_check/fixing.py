from __future__ import annotations


from .common import language_of, line_of, line_starts
from .patches import fix_patch_counts, patch_removal_range
from .settings import REPOSITORY


def removal_range(text, start, end):
    """Work out what to delete to take one comment out cleanly.

    A comment alone on its lines takes the lines with it. One after code takes
    the whitespace before it, and one before code the whitespace after it.

    @param text The file.
    @param start Where the comment starts.
    @param end Where it ends.
    @return (start, end, whole lines) of the text to delete.
    """
    line_start = text.rfind("\n", 0, start) + 1
    line_end = text.find("\n", end)
    line_end = len(text) if line_end < 0 else line_end
    before = text[line_start:start]
    after = text[end:line_end]
    if not before.strip() and not after.strip():
        return line_start, min(line_end + 1, len(text)), True
    if before.strip() and not after.strip():
        return start - (len(before) - len(before.rstrip())), end, False
    return start, end + (len(after) - len(after.lstrip())), False


def tidy_blank_lines(text, sites, language):
    """Clean up the blank lines around where whole comment lines came out.

    Two blank lines left together become one (two in Python), and a blank line
    left just inside a brace, or at the start of the file, goes.

    @param text The file after the comments came out.
    @param sites Offsets where whole lines were removed.
    @param language The file's language.
    @return The tidied text.
    """
    lines = text.split("\n")
    starts = line_starts(text)
    limit = 2 if language == "python" else 1
    drop = set()
    for site in sites:
        number = min(line_of(starts, site) - 1, len(lines) - 1)
        first = number
        while first > 0 and not lines[first - 1].strip():
            first -= 1
        last = number
        while last < len(lines) and not lines[last].strip():
            last += 1
        blanks = list(range(first, last))
        if not blanks:
            continue
        previous = lines[first - 1].rstrip() if first > 0 else ""
        following = lines[last].lstrip() if last < len(lines) else ""
        keep = limit
        if first == 0 or (language != "python" and (previous.endswith("{") or following.startswith("}"))):
            keep = 0
        if language == "python" and previous.endswith(":"):
            keep = 0
        drop.update(blanks[keep:])
    tidied = "\n".join(line for number, line in enumerate(lines) if number not in drop)
    return tidied.rstrip("\n") + "\n" if text.endswith("\n") else tidied


def fix_file(path, findings):
    """Take the fixable comments out of one file.

    @param path The file's path, relative to the repository.
    @param findings Its findings.
    @return How many comments came out.
    """
    file_path = REPOSITORY / path
    text = file_path.read_text(encoding="utf-8", errors="surrogateescape")
    language = language_of(path)
    ranges = []
    for finding in findings:
        if not finding.fixable:
            continue
        if finding.replacement:
            ranges.append((finding.start, finding.end, False, finding.replacement))
        elif language == "patch":
            ranges.append((*patch_removal_range(text, finding.start, finding.end), ""))
        else:
            ranges.append((*removal_range(text, finding.start, finding.end), ""))
    sites = []
    for start, end, whole, replacement in sorted(ranges, reverse=True):
        text = text[:start] + replacement + text[end:]
        sites = [site - (end - start) if site > start else site for site in sites]
        if whole:
            sites.append(start)
    if language == "patch":
        text = fix_patch_counts(text)
    else:
        text = tidy_blank_lines(text, sites, language)
    file_path.write_text(text, encoding="utf-8", errors="surrogateescape")
    return len(ranges)

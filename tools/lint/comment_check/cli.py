from __future__ import annotations

import argparse
import json
from pathlib import Path

from .files import scan_file, tracked_files
from .fixing import fix_file
from .settings import DESCRIPTION


def print_findings(findings):
    """Print each finding as path:line: kind: its first line.

    @param findings The findings.
    """
    for finding in findings:
        first = finding.text.strip().split("\n")[0][:100]
        note = "" if finding.fixable else " (not fixable: take it out by hand)"
        print(f"{finding.path}:{finding.line}: {finding.kind}{note}: {first}")


def write_report(findings, report_path):
    """Write every finding to a JSON file.

    @param findings The findings.
    @param report_path Where to write them.
    """
    rows = [{"path": f.path, "line": f.line, "kind": f.kind, "text": f.text, "context": f.context, "fixable": f.fixable}
            for f in findings]
    Path(report_path).write_text(json.dumps(rows, indent=1, ensure_ascii=False), encoding="utf-8")


def parse_arguments():
    """Read the command line.

    @return The parsed arguments.
    """
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("paths", nargs="*", help="files to check, relative to the repository (default: every tracked file)")
    parser.add_argument("--fix", action="store_true", help="take the comments out")
    parser.add_argument("--report", metavar="FILE", help="write every finding to FILE as JSON instead of listing them")
    return parser.parse_args()


def main():
    """Check the files, then list, report or fix what's found.

    @return 0 when nothing's found or everything was fixed, 1 otherwise.
    """
    arguments = parse_arguments()
    paths = arguments.paths or tracked_files()
    findings = []
    for path in paths:
        findings.extend(scan_file(path))
    if arguments.report:
        write_report(findings, arguments.report)
        print(f"{len(findings)} comments written to {arguments.report}")
        return 0
    if arguments.fix:
        by_path = {}
        for finding in findings:
            by_path.setdefault(finding.path, []).append(finding)
        removed = sum(fix_file(path, path_findings) for path, path_findings in by_path.items())
        left = [finding for finding in findings if not finding.fixable]
        print(f"Took out {removed} comments from {len(by_path)} files.")
        print_findings(left)
        return 1 if left else 0
    print_findings(findings)
    if findings:
        print(f"{len(findings)} comments. Only a doc block on a function is allowed; --fix takes them out.")
        return 1
    return 0

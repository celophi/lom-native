#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from lom_headless import HeadlessError, headless

DESCRIPTION = """Run one registered test in its own folder.

CTest runs every test through this script (see cmake/tests/registry.cmake).
For each test it:

- makes <run-dir>/<name>/ and runs the test there, writing its output to
  log.txt as well as the terminal, and the result to result.json;
- replaces @DIR@ in the arguments with that folder, @OUT@ with the prefix
  for any screenshots the test saves (<folder>/route), and @DISC@ with your
  extracted disc (LOM_DISC_ROOT, or the one the test names, like the
  Japanese disc for the Japanese game's tests);
- sets LOM_TEST_CLOCK, which picks the paced or the virtual clock, plus any
  variables the test asks for with --env, and LOM_NO_ERROR_DIALOG, so a game
  that can't start its graphics exits instead of waiting on a dialog;
- runs native tests through run-headless.sh, unless the test says not to
  (on Windows, with its settings made here instead: see lom_headless.py);
- passes when the exit code is the one the test expects.

A test that needs the disc fails, instead of skipping, when there's no disc,
so a missing disc can never look like a pass.
"""
CLOCKS = ("paced", "virtual")

SETUP_FAILURE = 90


def fail_setup(message: str) -> int:
    """Report that a test couldn't be set up.

    @param message Why.
    @return The exit code for that.
    """
    print(f"run-test: {message}", file=sys.stderr, flush=True)
    return SETUP_FAILURE


def expand(argument: str, substitutions: dict[str, str]) -> str:
    """Replace @DIR@, @DISC@ and the like in one argument."""
    for key, value in substitutions.items():
        argument = argument.replace(key, value)
    return argument


def run(command: list[str], test_dir: Path, env: dict[str, str]) -> int:
    """Run the command in test_dir, copying its output to log.txt. Returns its exit code."""
    with open(test_dir / "log.txt", "w") as log:
        process = subprocess.Popen(command, cwd=test_dir, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        assert process.stdout is not None
        for line in process.stdout:
            sys.stdout.write(line)
            log.write(line)
        code = process.wait()
    return 128 - code if code < 0 else code


def main() -> int:
    """Run one test in its own folder, headless, with its clock and environment, and report how it went.

    @return 0 when it passed.
    """
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--name", required=True, help="the test's name, used for its folder")
    parser.add_argument("--run-dir", type=Path, required=True, help="where test folders go (LOM_TEST_RUN_DIR overrides it)")
    parser.add_argument("--disc", type=Path, help="your extracted disc, for tests that use @DISC@")
    parser.add_argument("--no-headless", action="store_true", help="run without run-headless.sh")
    parser.add_argument("--expect-exit", type=int, default=0, help="the exit code that counts as a pass")
    parser.add_argument("--clock", choices=CLOCKS, default="paced", help="the clock the test runs on")
    parser.add_argument("--paced-only", action="store_true", help="always use the paced clock")
    parser.add_argument("--env", action="append", default=[], help="NAME=VALUE to set for the test; repeat for more")
    parser.add_argument("--capture-env", help="an environment variable to set to the screenshot prefix (@OUT@)")
    parser.add_argument("command", nargs=argparse.REMAINDER, help="-- then the test command")
    args = parser.parse_args()

    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        return fail_setup("no command")

    run_dir = Path(os.environ["LOM_TEST_RUN_DIR"]) if os.environ.get("LOM_TEST_RUN_DIR") else args.run_dir
    test_dir = run_dir / args.name
    shutil.rmtree(test_dir, ignore_errors=True)
    test_dir.mkdir(parents=True)

    uses_disc = any("@DISC@" in argument for argument in command)
    if uses_disc and (args.disc is None or not any((args.disc / executable).is_file() for executable in ("SLUS_010.13", "SLPS_021.70"))):
        return fail_setup(f"this test needs your extracted disc; set LOM_DISC_ROOT (got {args.disc})")

    substitutions = {"@DIR@": str(test_dir), "@OUT@": str(test_dir / "route"), "@DISC@": str(args.disc or "")}
    command = [expand(argument, substitutions) for argument in command]

    clock = os.environ.get("LOM_TEST_CLOCK_OVERRIDE") or args.clock
    if clock not in CLOCKS:
        return fail_setup(f"unknown clock {clock}")
    if args.paced_only:
        clock = "paced"
    env = os.environ.copy()
    env["LOM_REMASTER_ROOT"] = ""
    for item in args.env:
        name, _, value = item.partition("=")
        env[name] = expand(value, substitutions)
    if args.capture_env:
        env[args.capture_env] = substitutions["@OUT@"]
    env["LOM_TEST_CLOCK"] = clock
    env["LOM_NO_ERROR_DIALOG"] = "1"

    try:
        full_command = command if args.no_headless else headless(command, env)
    except HeadlessError as error:
        return fail_setup(f"headless: {error}")
    start = time.monotonic()
    code = run(full_command, test_dir, env)
    passed = code == args.expect_exit

    result = {
        "name": args.name,
        "command": full_command,
        "clock": clock,
        "exit": code,
        "expected_exit": args.expect_exit,
        "passed": passed,
        "seconds": round(time.monotonic() - start, 1),
    }
    (test_dir / "result.json").write_text(json.dumps(result, indent=2) + "\n")

    if passed:
        return 0
    print(f"run-test: exit {code}, expected {args.expect_exit}", flush=True)
    return code if code not in (0, args.expect_exit) else 1


if __name__ == "__main__":
    sys.exit(main())

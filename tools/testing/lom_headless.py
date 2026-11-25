from __future__ import annotations

import os
from pathlib import Path

HEADLESS = Path(__file__).resolve().with_name("run-headless.sh")


class HeadlessError(Exception):
    pass


def headless(command: list[str], env: dict[str, str]) -> list[str]:
    """The command to run instead of command, with env changed to match.

    Raises HeadlessError if the settings asked for can't be used here.
    """
    if os.name != "nt":
        return [str(HEADLESS), *command]
    gpu = env.get("LOM_TEST_GPU", "hardware")
    if gpu != "hardware":
        raise HeadlessError(f"Windows uses the GPU's own driver, so LOM_TEST_GPU can't be '{gpu}'")
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env["SDL_VIDEODRIVER"] = "offscreen"
    env["SDL_AUDIODRIVER"] = "dummy"
    env["SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT"] = "0x0000/0x0000"
    env["LOM_NO_ERROR_DIALOG"] = "1"
    return list(command)

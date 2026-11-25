# Troubleshooting

## Logs

When you play through the launcher, everything the game prints goes to a log, one per run, named `game-<date>-<time>.log`. It keeps the last 10. The launcher's own log is `launcher.log` in the same folder. If the game crashes, the launcher tells you which log it wrote. The folder is:

- Linux: `~/.local/share/lom-native/logs`
- macOS: `~/Library/Application Support/lom-native/logs`
- Windows: `%LOCALAPPDATA%\lom-native\logs`
- Portable mode: `user\logs` next to the game

If you start the game yourself (`lom`, or `lom-jp` for the Japanese version), the output just goes to your terminal.

## Crashes

The log usually won't tell you where it crashed, but a backtrace will. On distros with systemd, a crash gets saved as a core dump, and you can open it in gdb with this (use `lom-jp` instead if it was the Japanese version):

```sh
coredumpctl gdb lom
```

Then type `bt`. The default build keeps debug info, so you get function names and line numbers.

On macOS, a crash report with the backtrace goes to `~/Library/Logs/DiagnosticReports`, and Console shows it too.

On Windows there's no core dump to open afterwards, so start the game under LLDB, which comes with MSYS2's Clang: `lldb -- build/opt/lom.exe /path/to/extracted/disc`, then `run`, and `bt` when it stops.

## Reporting a bug

If the game crashes or stops with an error, the launcher offers to report it. For anything else, like a glitch or getting stuck, close the game and use Help > Report a bug. Either way the launcher saves a zip in its `bug-reports` folder and opens a new issue on GitHub in your browser. Drag the zip into the issue and say what you were doing.

The zip has:

- the game's log, a crash report if it crashed (where it crashed, the registers and a backtrace), and a description of your PC and setup: OS, CPU, RAM, graphics card and driver, displays, audio, controllers and the game's settings
- the launcher's log and settings
- your memory cards
- the session recording, if you turned it on

Your home folder is replaced with `~` in the text files, but there can be other paths in there, like where your disc is. Nothing is sent anywhere until you post the issue, so have a look first if you want.

If you run the game without the launcher, it still writes its crash and system reports into the logs folder above. Attach those, and say which commit you built (`git rev-parse --short HEAD`).

For me: `tools/triage/open_report.py` unpacks a report and prints the summary, the machine and the crash, and with `--binary` and that release's game executable it turns the backtrace into function names.

## Session recording

Session recording is off by default. If you turn it on, the game saves your controller input and its timing as you play, and a bug report includes it. The game can't play a recording back yet; that's still to come, and the recording is what it'll need. In the launcher it's "Record play sessions for bug reports" in the settings. Running the game yourself, set `LOM_RECORD=1`.

From the launcher, each run gets its own folder in `sessions`, next to the launcher's `logs`. Otherwise it's in the game's own data:

- Linux: `~/.local/state/lom-native/sessions`
- macOS: `~/Library/Application Support/lom-native/sessions`
- Windows: `%LOCALAPPDATA%\lom-native\sessions`
- Portable mode: `user\sessions` next to the game

`LOM_SESSION_DIR` puts them somewhere else. A session is about 16 MB an hour. The game keeps the last 20, and once the older ones add up to 50 MB it deletes the oldest, so it can't fill your disk. The newest one always stays. In each folder:

- `log.txt` is everything the game printed.
- `end` is how the run ended: `quit`, `exit`, or a signal like `signal 11` if it crashed. If it still says `running`, the game froze or got killed.
- `inputs.bin` is your controller input and timing.
- `manifest.json` is the build, where your disc was, and which mods and switches were on.
- `card/` is a copy of your memory card from when the run started.

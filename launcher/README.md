# The launcher

The launcher is how you get from your own Legend of Mana disc to playing. You point it at a disc image or a folder you've already extracted, it checks the files against the US and Japanese discs it knows, sets them up, and starts the game. It also looks after your save folders and imports memory cards from emulators.

## How it's laid out

- `core/` is a C++17 library with the rules for which discs and files are accepted. It has no GUI, network or process code, so another frontend could use it. The headers are in `include/lom/launcher/` and the code in `src/`, both split the same way: `disc/` reads, checks and installs the game files, `saves/` handles memory cards and saves, and `common/` has the rest.
- `app/` is the window itself, `lom-launcher`, in Qt 6 Widgets. `window/` has the main window and what it runs on, `pages/` the pages it shows, and `dialogs/` the Settings, Game files, Mods and memory card dialogs. `services/` is everything that talks to the outside world: files, processes, the desktop, update checks, installing updates and release notes. Most services are an interface plus the real thing, so the tests can swap in their own.
- `cli/` is `lom-disc-tool`, the core on the command line. I use it to make the reference data and to check discs without the window. It has `identify`, `manifest`, `project-iso`, `verify-folder`, `check-folder`, `install` and `card`.
- `data/` is what the launcher ships with: `supported-discs.json`, the manifests, and the mods that come with lom-native, in `mods/`.
- `tests/` has a test for what the launcher does on each thing you can click (`behavior/`), unit tests (`unit/`), tests of the real process runner against a stand-in game (`providers/`), and the made-up discs and cards they share (`support/`).

## Building it

The main build includes the launcher (`LOM_BUILD_LAUNCHER`, on by default) and puts `lom-launcher` next to the games, `lom` and `lom-jp`:

```sh
cmake --preset opt
cmake --build --preset opt
./build/opt/lom-launcher
```

If you're only working on the launcher, you can build it on its own, without the game or anything the game needs:

```sh
cmake -S launcher -B build/launcher -DCMAKE_BUILD_TYPE=Release
cmake --build build/launcher
```

The window needs Qt 6.5 or newer (Widgets and Network). Without Qt you still get the core, `lom-disc-tool` and the core's tests. The tests use GoogleTest and gMock if you have them, and download them if you don't.

There are three build options:

- `LOM_APP_VERSION` is the version the launcher shows and compares when it checks for updates. Leave it empty and it works it out from git: `2.3.1` on the `v2.3.1` tag, or `2.3.1+3.gc76f211` three commits after it. Release builds set it exactly.
- `LOM_UPDATE_REPOSITORY` is the GitHub repository it checks for releases, their notes and the files it updates itself from, `celophi/lom-native` by default. Empty turns off the update check, installing updates and What's new. The Flatpak does that, since it has no network access.
- `LOM_LAUNCHER_TEST_IMAGE` is your own US disc image (CUE or BIN), for the `launcher-real-image` test. Empty by default.

When I'm working on it, two environment variables help. `LOM_GAME_EXECUTABLE` starts a different game executable instead of the one next to the launcher (`lom`, or `lom-jp` for the Japanese version). `LOM_LAUNCHER_DATA` reads `supported-discs.json` from another folder instead of the `launcher-data` next to it.

## Where things go

On Linux:

- Its settings: `~/.config/lom-native/launcher.json`
- Game files it extracted: `~/.local/share/lom-native/games/<release>/<raw-sha1>/<installation>/data`, unless you chose another folder
- US saves: `~/.local/share/lom-native/cards`, where the game already kept them
- Japanese saves: `~/.local/share/lom-native/cards-jp`
- Logs: `~/.local/share/lom-native/logs`
- What it last checked: `~/.local/share/lom-native/verification`

The paths come from Qt's standard locations, so Windows and macOS use their own per-user folders. A folder you'd already extracted stays where it is, and the launcher only reads it.

Setting up a disc hashes every file once. After that the launcher keeps each file's size and modified time, and the next time it opens it only hashes the files where those changed, though it still checks every file is there. Game files > Verify the selected game's files hashes everything again.

## The disc data

`data/supported-discs.json` says which discs it takes: each real disc's serial and its raw image's SHA-1. Leaving one empty or null turns that way of setting up off.

A raw image (a BIN, or a CUE pointing at one) is accepted by its exact SHA-1. If it doesn't match, say it's a different dump of the same disc, the launcher checks its files instead. That's the same check an ISO (2048-byte sectors) or an extracted folder gets: every file has to match the disc's manifest. So a disc without a manifest only works as its exact raw image.

I make a manifest from a raw image whose SHA-1 the catalog already accepts:

```sh
build/opt/launcher/lom-disc-tool manifest "Legend of Mana (USA).cue" \
    --catalog launcher/data/supported-discs.json --release us \
    --out launcher/data/manifests/us-slus-01013.json
```

The Japanese one I made from a 2048-byte image instead. With `--executable-sha1`, `manifest` takes one of those as long as its boot executable has the real disc's SHA-1, and the manifest's `generator` says that's how it was made. Don't edit a manifest by hand. Make it again.

To check the ISO path, `project-iso` turns a raw image into a 2048-byte one:

```sh
build/opt/launcher/lom-disc-tool project-iso "Legend of Mana (USA).bin" us.iso
build/opt/launcher/lom-disc-tool install us.iso --catalog launcher/data/supported-discs.json --games-root /tmp/check
```

## Languages

The launcher's English text is in `app/window/ui_text.h`, each piece with a name. `app/translations/<code>.json` has the other seven of the Remaster's eight languages (`de`, `fr`, `es`, `ja`, `ko`, `zh_CN`, `zh_TW`), under the same names. Anything missing shows in English.

If you haven't picked a language in its settings, the launcher uses the first of your system's languages it has: Windows' display languages, macOS's Language & Region list (the Mac app lists the eight in `CFBundleLocalizations`), or `LANGUAGE` and `LANG` on Linux. It passes its language to the game in `LOM_LANGUAGE`, so the game's F1 menu matches.

`launcher-specs` checks every language has every piece of text, with the same `%1`, `%2` and so on to fill in. So a new piece of text needs a line in each file.

The translations aren't from native speakers, so if you speak one of these languages, corrections are welcome. The Details on a problem page and the bug report's summary stay in English, since they're for whoever reads the report.

## Tests

- `launcher-unit` needs GoogleTest. Unit tests on made-up discs, cards and saves.
- `launcher-specs` needs Qt, GoogleTest and gMock. What the launcher does for each thing you can do, with the outside world swapped out and no real window.
- `launcher-providers` needs Qt and GoogleTest. The real process runner, against a stand-in game.
- `launcher-real-image` needs `LOM_LAUNCHER_TEST_IMAGE`. It identifies your own US image, and never copies any game data.

The first three don't need a display, a disc or the network. To run them all:

```sh
cmake --preset opt
cmake --build --preset opt
ctest --test-dir build/opt -R '^launcher-' --output-on-failure
```

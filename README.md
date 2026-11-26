# lom-native

This is a native source port of Legend of Mana, built from [lom-decomp](https://github.com/celophi/lom-decomp).

It builds and runs on Linux, macOS and Windows, and you can get into the game and play. It isn't done, though. It hasn't been played all the way through yet, and there are still bugs, so expect things to break. The state of everything is just experimental at the moment.

## Setup

You need your own copy of Legend of Mana, the US version (SLUS-01013) or the Japanese one (SLPS-02170), as a disc image or an already extracted folder. The launcher works out which one you have and runs the matching game. If you have both, set up both, and pick which one to play under Game files. The Japanese one plays as it did in Japan, so Circle confirms and Cross cancels, and its text is always the disc's own Japanese: the Remaster's text options don't apply to it. You also need a graphics card the game can draw with: one with Vulkan on Linux, one with Direct3D 12 on Windows, or any Apple Silicon Mac.

Grab the latest from the [releases page](https://github.com/celophi/lom-native/releases):

- Linux: the AppImage, or the Flatpak
- macOS: the `.dmg`, for Apple Silicon Macs with macOS 13 or newer
- Windows: the zip, for 64-bit Windows 10 or 11

Each one comes with everything it needs. Open it and point the launcher at your disc image or folder. It checks the files against the real disc, sets them up, and then you can hit Play. F1 or the controller's Guide button opens the port's settings.

The apps aren't signed, so macOS and Windows will warn you the first time you open them. [docs/installing.md](docs/installing.md) has how to get past that, plus the Flatpak, the Steam Deck and portable mode.

## Features

It plays the PS1 game. On top of that, the settings have a few things the original doesn't:

- Widescreen, in towns, dungeons and fights!
- Scaling and internal resolution!
- Controls remapping!
- Separate volumes for the music and the sound effects!
- The launcher and the F1 settings in English, German, French, Spanish, Japanese, Korean or Chinese!

### Note Regarding Remaster

If you also own the Legend of Mana Remaster on Steam, the game can use parts of it. The launcher finds it in Steam, or you can choose its folder under Game files. The game reads it where it's installed, so nothing is copied. These only work with it:

- The Remaster's repainted backgrounds, and in widescreen its wider ones
- Its repainted faces in conversations and its encyclopedia pictures
- The game's own text in German, French, Spanish, Japanese, Korean or Chinese, or its revised English. US disc only; without the Remaster it's the PS1's English.
- Its soundtrack or its arranged soundtrack, instead of the PS1's
- Its recordings of the sound effects

[docs/settings.md](docs/settings.md) explains each setting.

## Mods

The launcher's Mods button lists your mods, and you turn each one on or off there. A few come with it, turned off:

- Always run, so you hold the button to walk instead
- Quick fights, which skips the results window after a fight
- No natural healing, for a harder game
- Skip movies
- Skip or win battles, which skips regular fights or wins them as soon as they start, and wins boss fights too if you like, for following just the story
- Japanese balance, which plays the US version with the Japanese version's harder combat and items

At this time, I'm still building out a modding framework, so these are a proof of concept initially.

[docs/modding.md](docs/modding.md) has how mods work and how to make your own.

## Building

It builds on Linux, on macOS with Apple Silicon, and on 64-bit Windows, with Clang and CMake. On Linux, once you have what it needs, it's:

```sh
git clone https://github.com/celophi/lom-native.git
cd lom-native
sh tools/ci/checkout-submodules.sh
cmake --preset opt
cmake --build --preset opt
./build/opt/lom-launcher
```

[docs/building.md](docs/building.md) has what to install first, the macOS and Windows steps, the release packages and the tests.

## Bug Reporting

If the game crashes or stops with an error, the launcher offers to report it. For anything else, like a glitch or getting stuck, close the game and use Help > Report a bug. Either way the launcher saves a zip and opens a new issue on GitHub in your browser. Drag the zip into the issue and say what you were doing. Nothing is sent anywhere until you post the issue.

[docs/troubleshooting.md](docs/troubleshooting.md) has what's in the zip, where the logs are, and how to get a backtrace from a crash.

## License

MIT; see [LICENSE](LICENSE). The patches in `patches/psyz/` that modify PSY-Z's MPL-2.0 files are MPL-2.0; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). No game data is included: you need your own copy of Legend of Mana.

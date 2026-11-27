# Third-party notices

lom-native is MIT-licensed ([LICENSE](LICENSE)), except as listed here.

## In this repository

### The game's code (`game/`)

Forked from [lom-decomp](https://github.com/celophi/lom-decomp), MIT,
Copyright (c) 2026 Christopher Harris (celophi). Its
[LICENSE](game/LICENSE) and [notices](game/THIRD_PARTY_NOTICES.md) are kept
in `game/`. In short, from those notices: the MIT license covers only the
decomp's own work. It grants no rights to the original game's assets (its
graphics, audio, music, video, text or other data), to Square Enix's
trademarks and names, or to anything copied from the game. lom-decomp and
lom-native aren't affiliated with or endorsed by Square Enix.

### PSY-Z patches (`patches/psyz/`)

Patches to PSY-Z files are licensed as the files they modify: MPL-2.0 for
`psyz/src/platform/` and `psyz/src/psyz/`, MIT for `decomp/`, and zlib for
PSY-Z's copy of SDL (`external/SDL/`). Three change `psyz/include/`, which
has no published license (below). See
[patches/psyz/README.md](patches/psyz/README.md) for the scope and
[patches/psyz/LICENSE](patches/psyz/LICENSE) for the MPL-2.0 text.

### xBR shader (`src/gpu/shaders/xbr.frag.glsl`, `src/gpu/shaders/xbr.frag.metal`, `src/gpu/shaders/xbr.frag.hlsl`, `src/gpu/shaders/xbr_spv.h`, `src/gpu/shaders/xbr_msl.h`, `src/gpu/shaders/xbr_dxil.h`)

Hyllian's xBR-lv2 shader, MIT. Copyright (C) 2011-2016 Hyllian. The full
notice is kept at the top of each of these files. The Metal and HLSL versions
are ports of the GLSL one, for macOS and Windows.

### PSY-Z's drawing shaders (`src/gpu/shaders/psx.vert.glsl`, `src/gpu/shaders/psx.frag.glsl` and the `src/gpu/shaders/lom_psx_*.h` built from them)

PSY-Z's own `psx.vert.glsl` and `psx.frag.glsl` (`psyz/src/platform/shaders`),
with my additions for the Legend of Mana Remaster's HD pages. Like the rest of
`psyz/src/platform/`, they're MPL-2.0 ([patches/psyz/LICENSE](patches/psyz/LICENSE)),
and so are the headers `tools/build/compile_renderer_shaders.py` builds from
them.

### Menu fonts (`src/ui/fonts/`)

The in-game menu's fonts, all under the SIL Open Font License 1.1:

- Alegreya, Copyright 2011 The Alegreya Project Authors, with no Reserved
  Font Name. The two files here
  are static instances of Google Fonts' variable font, Bold and Medium Italic,
  cut down to Latin text by `tools/build/make_menu_fonts.py`. License:
  [packaging/licenses/alegreya-OFL.txt](packaging/licenses/alegreya-OFL.txt).
- Atkinson Hyperlegible, Copyright 2020 Braille Institute of America, Inc.,
  with no Reserved Font Name, unchanged. License:
  [packaging/licenses/atkinson-hyperlegible-OFL.txt](packaging/licenses/atkinson-hyperlegible-OFL.txt).
- Noto Sans CJK, © 2014-2021 Adobe, with Reserved Font Name "Source" (which
  these files don't use). The eight files here are its Japanese, Korean and
  Simplified and Traditional Chinese faces, Regular and Bold, cut down to the
  characters the menu's translations use by
  `tools/build/make_menu_fonts.py --cjk`. License:
  [packaging/licenses/noto-sans-cjk-OFL.txt](packaging/licenses/noto-sans-cjk-OFL.txt).

## Used at build time (git submodules, not copied here)

The game executable is built from, and links, these projects. Their source is
fetched as submodules under `external/`, and each keeps its own license.

| Project | License | Copyright |
| --- | --- | --- |
| [PSY-Z](https://github.com/Xeeynamo/psyz): `psyz/src/platform/`, `psyz/src/psyz/` | MPL-2.0 | PSY-Z contributors |
| PSY-Z: `decomp/` | MIT | github.com/psyqz members; github.com/xeeynamo/sotn-decomp contributors |
| PSY-Z: `psyz/include/` | No license published | PSY-Z contributors |
| [SDL 3](https://github.com/libsdl-org/SDL), via PSY-Z | zlib | Sam Lantinga |
| [Lua 5.4](https://www.lua.org/) (`external/lua`), for mods' scripts | MIT | Lua.org, PUC-Rio |
| [Dear ImGui](https://github.com/ocornut/imgui), via PSY-Z's cimgui | MIT | Omar Cornut |
| [PCSX-Redux](https://github.com/grumpycoders/pcsx-redux) OpenBIOS SJIS lookup (`openbios/charset/sjis.c`), via PSY-Z's nugget | MIT | PCSX-Redux authors |
| Shinonome 14-dot font (`openbios/charset/font*.raw`), via PSY-Z's nugget | Public domain | Yasuyuki Furukawa, /efont/; converted by Wei Mingzhi |

The memory card screens use the last two, compiled into the game, for the
BIOS font.

PSY-Z's own LICENSE file lists `psyz/include/` as unlicensed (along with
`psyz/src/psyq/`, which isn't in the version I pin). Those are the PS1 SDK
headers the game and PSY-Z are built against: types, structures, function
declarations and macros, with no code of their own, so only what the macros
and type layouts turn into ends up in the game. It's still the one part of
what I build on that has no published license, and I'd like that sorted out
upstream.

The launcher links [Qt 6](https://www.qt.io/), which is LGPL-3.0 (or GPL, or
commercial). When you build it yourself, it's your system's Qt, linked
dynamically, not part of this repository. The release packages are below.

A binary distribution must include the MIT notices above (`game/`, PSY-Z
`decomp/`, Dear ImGui, PCSX-Redux, xBR, Lua) and the fonts' OFL texts, must make the source of the MPL-2.0
files, including these patches, available, as MPL-2.0 section 3.2 requires,
and a launcher shipped with Qt must meet the LGPL-3.0's terms for it. The
next section is how the release packages do that.

## In the release packages

Each release package (`packaging/`) carries every license and notice in this
file, in `share/licenses/lom-native/` on Linux, in the app's
`Contents/Resources/licenses/` on macOS, and in `licenses/` on Windows.

The MPL-2.0 source they're built from, PSY-Z's `psyz/src/platform/` and
`psyz/src/psyz/` with my patches to them, is this repository at the release's
tag, with PSY-Z pinned as its submodule.

They also ship a few things this repository doesn't contain:

- ffmpeg, from FFmpeg 9.0.2, LGPL-2.1-or-later. The game runs it as a separate
  program to decode its movies, and the Remaster's (H.264 and AAC in MP4,
  and one WMA track) when you have it. It's built from FFmpeg's unmodified release
  with only the parts the game uses, and with no GPL or nonfree parts
  (`packaging/ffmpeg/build.sh`). Every release attaches the exact source
  tarball it was built from.
- The AppImage bundles Qt 6.8.3 (LGPL-3.0), the ICU 73.2 that comes with it
  (Unicode license), and the Ubuntu 22.04 libraries Qt needs, each with its
  Debian copyright file in `share/doc/`. Qt is unmodified and linked
  dynamically, so you can unpack the AppImage and swap in your own.
- The Flatpak gets Qt from the KDE runtime, so it doesn't include Qt itself.
- The Mac app bundles Qt 6.10.3's frameworks (LGPL-3.0), unmodified and
  linked dynamically. It uses macOS's own system libraries for everything
  else.
- The Windows zip bundles MSYS2's builds of Qt and the libraries it and the
  game load, like libc++, ICU and zstd. Some carry MSYS2's patches, Qt
  among them. Each ships with its package's license files, and
  `licenses/msys2/PACKAGES.txt` lists the exact package versions with links
  to the source MSYS2 built them from, patches included. They're all DLLs
  next to the programs, so you can swap in your own builds.

## Not included

No game data. Legend of Mana is © Square Enix. Running the port requires your
own copy of the game.

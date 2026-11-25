# Building

The game builds and runs on Linux, on macOS with Apple Silicon, and on 64-bit Windows.

## What you need

On Linux:

- Clang. It's the only compiler I build and test with.
- CMake 3.25 or newer.
- Python 3 with libclang's Python bindings. Some distros ship them with clang (Arch does). Otherwise, `pip install -r tools/requirements.txt`.
- git. The build uses it to apply the patches.
- glibc's 32-bit headers. The build scripts read the decomp as 32-bit PS1 code, which pulls them in. That's `lib32-glibc` on Arch (from multilib) and `libc6-dev-i386` on Debian and Ubuntu.
- Qt 6.5 or newer (Widgets and Network) if you want the launcher window. Without it you still get the game and `lom-disc-tool`.
- FFmpeg, for the movies.

SDL comes with PSY-Z, and the launcher tests download GoogleTest if you don't have it.

On macOS:

- An Apple Silicon Mac. I don't support Intel Macs.
- A current Xcode. I test with Xcode 26.6 on macOS 26.5.1, and I don't try to support older ones. The build uses Xcode's own Clang and libclang.
- CMake 3.25 or newer.
- Python 3 with libclang's Python bindings: `pip install -r tools/requirements.txt`, in a virtual environment. On macOS the build points them at Xcode's libclang, since the one pip brings along can't parse the game.
- Qt 6.5 or newer, if you want the launcher.
- FFmpeg, for the movies.

With Homebrew that's `brew install cmake qt ffmpeg`. `tools/ci/build-and-test-macos.sh` does the whole thing the way CI does.

On Windows you'll need [MSYS2](https://www.msys2.org), and you build in its CLANG64 shell. It has Clang, CMake, Python with libclang's bindings, Qt and FFmpeg:

```sh
pacman -S git mingw-w64-x86_64-binutils mingw-w64-clang-x86_64-{toolchain,cmake,ninja,python,python-yaml,qt6-base,ffmpeg,gtest,glslang}
```

`mingw-w64-x86_64-binutils` is GNU ld from MSYS2's MINGW64 environment. The build uses it for one step LLVM's linker can't do on Windows (`tools/build/link_units.py`). I build with Windows 11 and a current MSYS2, and I don't try to support older ones. `tools/ci/build-and-test-windows.sh` does the whole thing the way CI does.

On Windows, before you clone, tell git to leave line endings alone, or the build's patches won't apply: `git config --global core.autocrlf false`.

## Getting the code

Clone it and grab PSY-Z and Lua, with the submodules of PSY-Z the build uses. The game's code is in `game/`, forked from lom-decomp, so it comes with the clone:

```sh
git clone https://github.com/celophi/lom-native.git
cd lom-native
sh tools/ci/checkout-submodules.sh
```

`git submodule update --init external/lua` and `git submodule update --init --recursive external/psyz` work too, but the second fetches a few more that the build doesn't need, from other hosts.

## Building it

```sh
cmake --preset opt
cmake --build --preset opt
```

On Windows, pass `-G Ninja` too: `cmake --preset opt -G Ninja`, then `cmake --build --preset opt`. MSYS2's Python already has the bindings, so there's no virtual environment to make.

If you installed libclang in a virtual environment, activate it before running `cmake`, or CMake will pick up your system Python and fail. On macOS, pass it to CMake instead, since Xcode's Python comes first otherwise, and tell it where Qt is:

```sh
cmake --preset opt -DPython3_EXECUTABLE=$PWD/.venv/bin/python -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
cmake --build --preset opt
```

`runtime` builds the same thing without tests, and `debug` is a debug build. `opt32` is a 32-bit x86 build; it needs the 32-bit glibc headers and GCC runtime to build (`lib32-gcc-libs` on Arch, `gcc-multilib` on Debian and Ubuntu) and 32-bit Vulkan to run. It's Linux only, since macOS has no 32-bit.

If the build stops while it generates the game's globals (`tools/build/generate_unit_globals.py`), check you have the 32-bit glibc headers on Linux, a current Xcode on macOS, or that you're in MSYS2's CLANG64 shell on Windows.

## Running your build

The easiest way is the launcher:

```sh
./build/opt/lom-launcher
```

On macOS it's an app, `open build/opt/lom-launcher.app`. It finds the game next to the app.

On Windows, start it from the CLANG64 shell, `./build/opt/lom-launcher.exe`. The game and the launcher use MSYS2's DLLs (Qt, the C++ library) and its `ffmpeg`, which that shell has on its `PATH`.

Point it at your disc image or folder. It checks the files against the real disc, sets them up, and then you can hit Play.

You can also skip the launcher and start the game on an extracted folder directly:

```sh
./build/opt/lom /path/to/extracted/disc
```

## Tests

```sh
ctest --test-dir build/opt
```

That runs the launcher tests and the port's own tests. Each one checks one piece, like the GPU, the CD code or a menu, and gets its own folder under `build/opt/test-results/` with its log.

Some of them read your disc. Point CMake at your extracted disc when you configure:

```sh
cmake --preset opt -DLOM_DISC_ROOT=/path/to/extracted/disc
```

Without it those tests fail with a missing-disc error. To skip them instead, run `ctest --test-dir build/opt -LE disc`. The build makes the Japanese game too (`lom-jp`), and if you have its disc, `-DLOM_DISC_ROOT_JP=/path/to/extracted/japanese/disc` adds its tests: `routes-jp`, which plays it, `gte-tables-jp`, and `japanese-balance-data`, which checks the Japanese balance mod's numbers still match both discs. The route tests, which play through parts of the game, take about an hour; `-LE routes` leaves them out.

Most of them run headless through `tools/testing/run-headless.sh`. On Linux that needs a Vulkan driver even though nothing is shown. On a machine without a GPU, install Lavapipe and run them with `LOM_TEST_GPU=software`. On macOS they draw through Metal, and there's no software option. On Windows they draw through Direct3D 12, and the wrapper runs through MSYS2's `bash`.

## Release packages

Pushing a version tag makes CI build the Linux AppImage and Flatpak, the macOS `.dmg` and the Windows zip, and attach them to a draft release (`.github/workflows/release.yml`). You can build the Linux ones yourself with Docker, the same way CI does:

```sh
docker run --rm -v "$PWD:/src:ro" -v "$PWD/out:/out" -w /src ubuntu:22.04 \
    sh packaging/appimage/build.sh 1.0.0 /out
docker run --rm --privileged -v "$PWD:/src:ro" -v "$PWD/out:/out" -w /src \
    ghcr.io/flathub-infra/flatpak-github-actions:kde-6.11 sh packaging/flatpak/build.sh 1.0.0 /out
```

On an Apple Silicon Mac with Xcode, `sh packaging/macos/build.sh 1.0.0 out` builds the `.dmg`. On Windows, in MSYS2's CLANG64 shell with the packages above plus `make` and `mingw-w64-clang-x86_64-qt6-tools`, `sh packaging/windows/build.sh 1.0.0 out` builds the zip.

They all come with their own ffmpeg, a small LGPL build that only decodes the game's movies (`packaging/ffmpeg/build.sh`). The game uses an ffmpeg next to it before the one on your `PATH`.

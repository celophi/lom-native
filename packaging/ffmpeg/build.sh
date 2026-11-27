#!/bin/sh
set -eu

version=9.0.2
tarball=ffmpeg-$version.tar.xz
url=https://ffmpeg.org/releases/$tarball
sha256=8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e

source=
if [ "${1:-}" = --source ]; then
    source=$2
    shift 2
fi
if [ $# -ne 1 ]; then
    echo "usage: $0 [--source DIR] OUT" >&2
    exit 2
fi
mkdir -p "$1"
out=$(cd "$1" && pwd)

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

if [ -z "$source" ]; then
    cache=${FFMPEG_CACHE:-$out}
    mkdir -p "$cache"
    if [ ! -f "$cache/$tarball" ]; then
        curl -sSfL -o "$cache/$tarball.part" "$url"
        mv "$cache/$tarball.part" "$cache/$tarball"
    fi
    if command -v sha256sum > /dev/null; then
        actual=$(sha256sum "$cache/$tarball" | cut -d' ' -f1)
    else
        actual=$(shasum -a 256 "$cache/$tarball" | cut -d' ' -f1)
    fi
    if [ "$actual" != "$sha256" ]; then
        echo "$tarball doesn't match its pinned SHA-256 (got $actual)" >&2
        exit 1
    fi
    tar -xJf "$cache/$tarball" -C "$work"
    source=$work/ffmpeg-$version
fi
source=$(cd "$source" && pwd)

mkdir "$work/build"
cd "$work/build"

"$source/configure" \
    --cc="${CC:-clang}" \
    --extra-version=lom-native \
    --disable-everything \
    --disable-autodetect \
    --disable-doc \
    --disable-network \
    --disable-ffplay \
    --disable-ffprobe \
    --disable-avdevice \
    --disable-x86asm \
    --enable-static \
    --disable-shared \
    --enable-protocol=file,pipe \
    --enable-demuxer=str,mov,xwma \
    --enable-decoder=mdec,h264,aac,wmav2 \
    --enable-parser=h264,aac \
    --enable-encoder=rawvideo,pcm_s16le \
    --enable-muxer=rawvideo,pcm_s16le \
    --enable-filter=scale,fps,format,null,aformat,anull,aresample \
    > configure.log 2>&1 || { tail -40 configure.log >&2; exit 1; }

if ! grep -q '^#define FFMPEG_LICENSE "LGPL version 2.1 or later"$' config.h; then
    grep FFMPEG_LICENSE config.h >&2
    echo "ffmpeg isn't LGPL 2.1 or later with this configuration" >&2
    exit 1
fi

make -j"${JOBS:-8}" > make.log 2>&1 || { tail -40 make.log >&2; exit 1; }

exe=
[ -f ffmpeg.exe ] && exe=.exe
cp "ffmpeg$exe" "$out/ffmpeg$exe"
cp "$source/COPYING.LGPLv2.1" "$out/COPYING.LGPLv2.1"

configuration=$(sed -n 's/^#define FFMPEG_CONFIGURATION "\(.*\)"$/\1/p' config.h)
cat > "$out/FFMPEG-NOTICE.txt" <<EOF
This package includes ffmpeg, from FFmpeg $version (https://ffmpeg.org).

The game runs it to decode its movies, and the Legend of Mana Remaster's
(MP4: H.264 video, AAC sound, and one WMA track) when you have the
Remaster. It's a separate
program, not linked into the game.

FFmpeg is licensed under the GNU Lesser General Public License, version 2.1
or later. The full text is in COPYING.LGPLv2.1, next to this file. This
build has no GPL or nonfree parts.

It's built, unmodified, from FFmpeg's own release:

  $url
  SHA-256 $sha256

That same file is attached to the GitHub release this package came from. It
was configured with:

  $configuration

and built by packaging/ffmpeg/build.sh in lom-native.
EOF

echo "built ffmpeg $version (LGPL 2.1 or later) into $out"

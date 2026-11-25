#!/bin/sh
set -eu

if [ $# -ne 2 ]; then
    echo "usage: $0 VERSION OUT" >&2
    exit 2
fi
version=$1
mkdir -p "$2"
out=$(cd "$2" && pwd)
src=$(pwd)
id=io.github.celophi.lom_native
work=$(mktemp -d)

sed -e "s|@VERSION@|$version|" \
    -e "s|path: \.\./ffmpeg/build.sh|path: $src/packaging/ffmpeg/build.sh|" \
    -e "s|path: \.\./\.\.$|path: $src|" \
    "packaging/flatpak/$id.yml" > "$work/$id.yml"

if [ -f /.dockerenv ] && [ -d /var/lib/flatpak ]; then
    cp -a /var/lib/flatpak /var/lib/flatpak.copy
    rm -rf /var/lib/flatpak
    mv /var/lib/flatpak.copy /var/lib/flatpak
fi
flatpak remote-add --system --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak-builder --system --disable-rofiles-fuse --install-deps-from=flathub --force-clean \
    --state-dir="$work/state" --repo="$work/repo" "$work/build" "$work/$id.yml"
flatpak build-bundle --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
    "$work/repo" "$out/lom-native-$version-linux-x86_64.flatpak" "$id"

rm -rf "$work"
echo "built $out/lom-native-$version-linux-x86_64.flatpak"

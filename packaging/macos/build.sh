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

minimum_macos=13.0
cmake_version=3.31.10
aqt_version=3.3.0
qt_version=6.10.3
work=$(mktemp -d)
qt=$work/qt/$qt_version/macos
app="$work/dmg/Legend of Mana Native.app"
name=lom-native-$version-macos-arm64.dmg
trap 'rm -rf "$work"' EXIT

python3 -m venv "$work/venv"
"$work/venv/bin/pip" install -q -r "$src/tools/requirements.txt" "cmake==$cmake_version" "aqtinstall==$aqt_version"
cmake=$work/venv/bin/cmake
(cd "$work" && "$work/venv/bin/aqt" install-qt mac desktop "$qt_version" clang_64 -O "$work/qt" > /dev/null)

export MACOSX_DEPLOYMENT_TARGET=$minimum_macos
sh "$src/packaging/ffmpeg/build.sh" "$work/ffmpeg"

"$cmake" --preset runtime -S "$src" -B "$work/build" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$minimum_macos" \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="$qt" \
    -DPython3_EXECUTABLE="$work/venv/bin/python" \
    -DLOM_APP_VERSION="$version" \
    -DLOM_UPDATE_REPOSITORY=celophi/lom-native \
    -DLOM_BUNDLED_FFMPEG="$work/ffmpeg"
"$cmake" --build "$work/build" -j"${JOBS:-8}"
"$cmake" --install "$work/build" --prefix "$work/stage" > /dev/null
mkdir -p "$work/dmg"
mv "$work/stage/lom-launcher.app" "$app"

iconset=$work/lom-native.iconset
icon=$src/packaging/icon/lom-native.png
mkdir -p "$iconset"
for size in 16 32 128 256 512; do
    sips -z $size $size "$icon" --out "$iconset/icon_${size}x${size}.png" > /dev/null
    sips -z $((size * 2)) $((size * 2)) "$icon" --out "$iconset/icon_${size}x${size}@2x.png" > /dev/null
done
iconutil -c icns "$iconset" -o "$app/Contents/Resources/lom-native.icns"

"$qt/bin/macdeployqt" "$app"

licenses=$app/Contents/Resources/licenses/qt
mkdir -p "$licenses"
cp "$src/packaging/licenses/LGPL-3.0.txt" "$src/packaging/licenses/GPL-3.0.txt" "$licenses/"
cat > "$licenses/QT-NOTICE.txt" <<EOF
This app includes Qt $qt_version (https://www.qt.io), which the launcher uses
for its window. Qt is licensed under the GNU Lesser General Public License,
version 3. The full text is in LGPL-3.0.txt, which builds on the GNU General
Public License, version 3, in GPL-3.0.txt.

These are Qt's own unmodified binaries, linked dynamically, in
Contents/Frameworks and Contents/PlugIns. You can replace them with your own
build of Qt.

Qt's source for this version is at:

  https://download.qt.io/archive/qt/${qt_version%.*}/$qt_version/submodules/
EOF

for framework in "$app"/Contents/Frameworks/*; do
    case ${framework##*/} in
        Qt*.framework) ;;
        *)
            echo "no license to ship for ${framework##*/}" >&2
            exit 1 ;;
    esac
done

find "$app" -type f -perm -u+x -o -type f -name '*.dylib' | while read -r file; do
    file "$file" | grep -q Mach-O || continue
    if otool -L "$file" | grep -E -q '/opt/homebrew|/usr/local'; then
        echo "${file#"$app"/} loads a library from Homebrew:" >&2
        otool -L "$file" | grep -E '/opt/homebrew|/usr/local' >&2
        exit 1
    fi
    needs=$(vtool -show-build "$file" 2>/dev/null | awk '$1 == "minos" { print $2; exit }')
    if [ -n "$needs" ] && [ "$(printf '%s\n%s\n' "$needs" "$minimum_macos" | sort -t. -k1,1n -k2,2n | tail -1)" != "$minimum_macos" ]; then
        echo "${file#"$app"/} needs macOS $needs, newer than $minimum_macos" >&2
        exit 1
    fi
done

codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"

ln -s /Applications "$work/dmg/Applications"
cp "$src/packaging/macos/read-me.txt" "$work/dmg/Read me first.txt"
rm -f "$out/$name"
hdiutil create -volname "Legend of Mana Native" -srcfolder "$work/dmg" -fs HFS+ -format UDZO "$out/$name" > /dev/null
echo "built $out/$name"

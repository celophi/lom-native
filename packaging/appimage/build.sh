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

llvm=23
cmake_version=3.31.10
aqt_version=3.3.0
qt_version=6.8.3
qt=/opt/qt/$qt_version/gcc_64
work=/tmp/appimage
appdir=$work/AppDir
tools=${TOOLS_CACHE:-$work/tools}
name=lom-native-$version-linux-x86_64.AppImage

export DEBIAN_FRONTEND=noninteractive
export PYTHONDONTWRITEBYTECODE=1

apt-get update -qq
apt-get install -y -qq --no-install-recommends \
    ca-certificates curl gnupg file git make xz-utils python3 python3-venv \
    libc6-dev-i386 \
    libx11-dev libxext-dev libxi-dev libxrandr-dev libxcursor-dev libxfixes-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libasound2-dev libpulse-dev \
    libgl-dev libegl-dev libfontconfig1 libfreetype6 libdbus-1-3 libglib2.0-0 \
    libxkbcommon-x11-0 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
    libxcb-render-util0 libxcb-shape0 libxcb-sync1 libxcb-xfixes0 libxcb-xkb1 libwayland-cursor0 libwayland-egl1 \
    > /dev/null

llvm_key=6084F3CF814B57C1CF12EFD515CF4D18AF4F7421
mkdir -p /etc/apt/keyrings
curl -sSfL https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor > /etc/apt/keyrings/llvm.gpg
if ! gpg --show-keys --with-colons /etc/apt/keyrings/llvm.gpg | grep -q "^fpr:::::::::$llvm_key:"; then
    echo "apt.llvm.org's key isn't the one I expect" >&2
    exit 1
fi
echo "deb [signed-by=/etc/apt/keyrings/llvm.gpg] https://apt.llvm.org/jammy/ llvm-toolchain-jammy-$llvm main" \
    > /etc/apt/sources.list.d/llvm.list
apt-get update -qq
apt-get install -y -qq --no-install-recommends clang-$llvm python3-clang-$llvm > /dev/null
export PATH="/usr/lib/llvm-$llvm/bin:$PATH"

python3 -m venv /opt/tools
/opt/tools/bin/pip install -q "cmake==$cmake_version" "aqtinstall==$aqt_version"
(cd /tmp && /opt/tools/bin/aqt install-qt linux desktop "$qt_version" linux_gcc_64 -O /opt/qt > /dev/null)

fetch() {
    if [ ! -f "$tools/$2" ]; then
        curl -sSfL -o "$tools/$2.part" "$1/$2"
        mv "$tools/$2.part" "$tools/$2"
    fi
    echo "$3  $tools/$2" | sha256sum -c --quiet
}
mkdir -p "$tools" "$work/bin"
fetch https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1 \
    linuxdeploy-x86_64.AppImage c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
fetch https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1 \
    linuxdeploy-plugin-qt-x86_64.AppImage 15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724
fetch https://github.com/AppImage/appimagetool/releases/download/1.9.1 \
    appimagetool-x86_64.AppImage ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
fetch https://github.com/AppImage/type2-runtime/releases/download/20251108 \
    runtime-x86_64 2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d
for tool in linuxdeploy linuxdeploy-plugin-qt appimagetool; do
    install -m 755 "$tools/$tool-x86_64.AppImage" "$work/bin/$tool-x86_64.AppImage"
done
export APPIMAGE_EXTRACT_AND_RUN=1

git config --global --add safe.directory '*'

sh "$src/packaging/ffmpeg/build.sh" "$work/ffmpeg"

/opt/tools/bin/cmake --preset runtime -S "$src" -B "$work/build" \
    -DCMAKE_PREFIX_PATH="$qt" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DPython3_EXECUTABLE=/usr/bin/python3 \
    -DLOM_APP_VERSION="$version" \
    -DLOM_UPDATE_REPOSITORY=celophi/lom-native \
    -DLOM_BUNDLED_FFMPEG="$work/ffmpeg"
/opt/tools/bin/cmake --build "$work/build" -j"${JOBS:-8}"
DESTDIR=$appdir /opt/tools/bin/cmake --install "$work/build" > /dev/null

licenses=$appdir/usr/share/licenses/lom-native/qt
mkdir -p "$licenses"
cp "$src/packaging/licenses/LGPL-3.0.txt" "$src/packaging/licenses/GPL-3.0.txt" "$licenses/"
cat > "$licenses/QT-NOTICE.txt" <<EOF
This AppImage includes Qt $qt_version (https://www.qt.io), which the launcher
uses for its window. Qt is licensed under the GNU Lesser General Public
License, version 3. The full text is in LGPL-3.0.txt, which builds on the GNU
General Public License, version 3, in GPL-3.0.txt.

These are Qt's own unmodified binaries, linked dynamically, in usr/lib and
usr/plugins. You can replace them with your own build of Qt: unpack the
AppImage with --appimage-extract and swap the libraries.

Qt's source for this version is at:

  https://download.qt.io/archive/qt/${qt_version%.*}/$qt_version/submodules/
EOF

id=io.github.celophi.lom_native
export QMAKE=$qt/bin/qmake
export LD_LIBRARY_PATH=$qt/lib
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;libqwayland-egl.so"
export NO_STRIP=1
for plugin in wayland-shell-integration/libxdg-shell.so \
    wayland-graphics-integration-client/libqt-plugin-wayland-egl.so \
    wayland-decoration-client/libbradient.so \
    wayland-decoration-client/libadwaita.so; do
    mkdir -p "$appdir/usr/plugins/${plugin%/*}"
    cp "$qt/plugins/$plugin" "$appdir/usr/plugins/$plugin"
done
cd "$work"
"$work/bin/linuxdeploy-x86_64.AppImage" --appdir "$appdir" \
    --deploy-deps-only "$appdir/usr/plugins/wayland-shell-integration" \
    --deploy-deps-only "$appdir/usr/plugins/wayland-graphics-integration-client" \
    --deploy-deps-only "$appdir/usr/plugins/wayland-decoration-client" \
    --executable "$appdir/usr/bin/lom-launcher" \
    --executable "$appdir/usr/bin/lom" \
    --executable "$appdir/usr/bin/lom-jp" \
    --executable "$appdir/usr/bin/ffmpeg" \
    --desktop-file "$appdir/usr/share/applications/$id.desktop" \
    --icon-file "$appdir/usr/share/icons/hicolor/512x512/apps/$id.png" \
    --exclude-library 'libstdc++.so*' \
    --exclude-library 'libgcc_s.so*' \
    --plugin qt

docs=$appdir/usr/share/doc
for lib in "$appdir"/usr/lib/*.so*; do
    file=${lib##*/}
    case $file in
        libQt6*) continue ;;
        libicu*)
            mkdir -p "$licenses"
            cp "$src/packaging/licenses/icu-73.2.txt" "$licenses/ICU-LICENSE.txt"
            continue ;;
    esac
    package=$(dpkg-query -S "/usr/lib/x86_64-linux-gnu/$file" "/lib/x86_64-linux-gnu/$file" 2>/dev/null | cut -d: -f1 | head -1)
    if [ -z "$package" ] || [ ! -f "/usr/share/doc/$package/copyright" ]; then
        echo "no license to ship for $file" >&2
        exit 1
    fi
    mkdir -p "$docs/$package"
    cp "/usr/share/doc/$package/copyright" "$docs/$package/copyright"
done
if ! python3 -c 'import json, sys
packages = json.load(open(sys.argv[1]))["packages"]
sys.exit(not any(p.get("name") == "ICU" and p.get("versionInfo") == "73.2" for p in packages))' \
    "$qt/sbom/qtbase-$qt_version.spdx.json"; then
    echo "Qt's ICU isn't 73.2 any more; update packaging/licenses" >&2
    exit 1
fi

ARCH=x86_64 "$work/bin/appimagetool-x86_64.AppImage" --no-appstream \
    --runtime-file "$tools/runtime-x86_64" "$appdir" "$out/$name"
chmod 755 "$out/$name"
echo "built $out/$name"

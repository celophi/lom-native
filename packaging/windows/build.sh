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
work=$(mktemp -d)
pkg="$work/Legend of Mana Native"
name=lom-native-$version-windows-x64.zip
trap 'rm -rf "$work"' EXIT

sh "$src/packaging/ffmpeg/build.sh" "$work/ffmpeg"

cmake --preset runtime -G Ninja -S "$src" -B "$work/build" \
    -DPython3_EXECUTABLE="$(command -v python)" \
    -DLOM_APP_VERSION="$version" \
    -DLOM_UPDATE_REPOSITORY=celophi/lom-native \
    -DLOM_BUNDLED_FFMPEG="$work/ffmpeg"
cmake --build "$work/build" -j"${JOBS:-8}"
cmake --install "$work/build" --prefix "$pkg" > /dev/null
mv "$pkg/lom-launcher.exe" "$pkg/Legend of Mana Native.exe"
cp "$src/packaging/windows/read-me.txt" "$pkg/Read me first.txt"

windeployqt6 --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime \
    --dir "$pkg" "$pkg/Legend of Mana Native.exe" > /dev/null

binaries() {
    find "$pkg" -name '*.exe' -o -name '*.dll'
}
while :; do
    added=
    for dll in $(binaries | while read -r file; do ldd "$file"; done | awk '$3 ~ /^\/clang64\/bin\// { print $3 }' | sort -u); do
        if [ ! -f "$pkg/${dll##*/}" ]; then
            cp "$dll" "$pkg/"
            added=1
        fi
    done
    [ -n "$added" ] || break
done

licenses=$pkg/licenses/msys2
mkdir -p "$licenses"
packages=$(binaries | while read -r file; do
    case $file in
        "$pkg"/*.exe) continue ;;
        "$pkg"/*/*.dll) original=/clang64/share/qt6/plugins/${file#"$pkg"/} ;;
        *) original=/clang64/bin/${file##*/} ;;
    esac
    pacman -Qqo "$original"
done | sort -u)
cat > "$licenses/PACKAGES.txt" <<EOF
The DLLs in this folder come from these MSYS2 packages (https://www.msys2.org).
Some are built with MSYS2's own patches. Each link is the exact source MSYS2
built the package from, patches included. Each package's license files are
in the folder named after it, next to this file.

EOF
for package in $packages; do
    short=${package#mingw-w64-clang-x86_64-}
    if [ -d "/clang64/share/licenses/$short" ]; then
        cp -r "/clang64/share/licenses/$short" "$licenses/$short"
    else
        files=$(pacman -Qlq "$package" | grep -i -E '/(licen[cs]e|copying|notice)[^/]*$' || true)
        if [ -z "$files" ]; then
            echo "no license to ship for $package" >&2
            exit 1
        fi
        mkdir -p "$licenses/$short"
        for file in $files; do
            cp "$file" "$licenses/$short/"
        done
    fi
    package_version=$(pacman -Q "$package" | cut -d' ' -f2)
    base=$(sed -n '/^%BASE%$/{n;p;}' /var/lib/pacman/local/"$package-$package_version"/desc)
    echo "$package $package_version  https://repo.msys2.org/mingw/sources/$base-$package_version.src.tar.zst" >> "$licenses/PACKAGES.txt"
done

for game in lom lom-jp; do
    status=0
    (cd "$pkg" && PATH=/c/Windows/System32:/c/Windows ./$game.exe one two > /dev/null 2>&1) || status=$?
    if [ "$status" -ne 2 ]; then
        echo "$game.exe didn't start on its own (exit $status); a DLL is probably missing" >&2
        exit 1
    fi
done
(cd "$pkg" && PATH=/c/Windows/System32:/c/Windows ./ffmpeg.exe -hide_banner -version > /dev/null)

rm -f "$out/$name"
python - "$work" "$out/$name" <<'EOF'
import os, sys, zipfile
root, target = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for folder, _, files in os.walk(os.path.join(root, "Legend of Mana Native")):
        for file in sorted(files):
            path = os.path.join(folder, file)
            archive.write(path, os.path.relpath(path, root))
EOF
echo "built $out/$name"

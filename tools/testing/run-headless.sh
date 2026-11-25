#!/bin/sh
set -eu

if [ "$#" -eq 0 ]; then
    echo "usage: $0 <command> [arguments...]" >&2
    echo "Optional: LOM_TEST_GPU=hardware|software, VK_DRIVER_FILES=/path/to/icd.json" >&2
    exit 2
fi

is_software_icd() {
    case $(basename "$1") in lvp_icd*.json|vk_swiftshader_icd.json) return 0 ;; esac
    return 1
}

gpu=${LOM_TEST_GPU:-hardware}
if [ "$(uname -s)" = Darwin ]; then
    if [ "$gpu" != hardware ]; then
        echo "headless: macOS only has Metal, so LOM_TEST_GPU can't be '$gpu'" >&2
        exit 2
    fi
    echo "headless: Metal (macOS)" >&2
elif case $(uname -s) in MINGW* | MSYS* | CYGWIN*) true ;; *) false ;; esac; then
    if [ "$gpu" != hardware ]; then
        echo "headless: Windows uses the GPU's own driver, so LOM_TEST_GPU can't be '$gpu'" >&2
        exit 2
    fi
    echo "headless: Direct3D 12 (Windows)" >&2
else
    if [ -n "${VK_DRIVER_FILES:-}" ]; then
        gpu=explicit
    else
        case $gpu in
        hardware)
            for icd in /etc/vulkan/icd.d/*.json /usr/share/vulkan/icd.d/*.json; do
                if [ -f "$icd" ] && [ -r "$icd" ] && ! is_software_icd "$icd"; then
                    VK_DRIVER_FILES=${VK_DRIVER_FILES:+$VK_DRIVER_FILES:}$icd
                fi
            done
            if [ -z "${VK_DRIVER_FILES:-}" ]; then
                echo "headless: no hardware Vulkan driver found; use LOM_TEST_GPU=software." >&2
                exit 2
            fi
            ;;
        software)
            for icd in \
                /usr/share/vulkan/icd.d/lvp_icd*.json \
                /usr/local/share/vulkan/icd.d/lvp_icd*.json \
                "${XDG_DATA_HOME:-${HOME}/.local/share}"/vulkan/icd.d/lvp_icd*.json
            do
                if [ -f "$icd" ] && [ -r "$icd" ]; then
                    VK_DRIVER_FILES=$icd
                    break
                fi
            done
            if [ -z "${VK_DRIVER_FILES:-}" ]; then
                echo "headless: no software Vulkan driver found; install Lavapipe or set VK_DRIVER_FILES." >&2
                exit 2
            fi
            ;;
        *)
            echo "headless: LOM_TEST_GPU must be hardware or software, not '$gpu'" >&2
            exit 2
            ;;
        esac
    fi
    export VK_DRIVER_FILES
    unset VK_ICD_FILENAMES
    echo "headless: Vulkan driver ($gpu): $VK_DRIVER_FILES" >&2
    if [ "$gpu" != software ]; then
        export LOM_TEST_GPU_PEER=${LOM_TEST_GPU_PEER:-$$}
    fi
fi

unset DISPLAY WAYLAND_DISPLAY
export SDL_VIDEODRIVER=offscreen
export SDL_AUDIODRIVER=dummy
export LOM_NO_ERROR_DIALOG=1
export SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x0000/0x0000
exec "$@"

#!/bin/sh
set -eu

if ! grep -q '^\[multilib\]' /etc/pacman.conf; then
    printf '\n[multilib]\nInclude = /etc/pacman.d/mirrorlist\n' >> /etc/pacman.conf
fi

pacman -Syu --noconfirm --needed \
    base-devel clang cmake python git lib32-glibc lib32-gcc-libs \
    libx11 libxext libxi libxrandr libxcursor libxfixes libxss libxtst libxkbcommon \
    wayland wayland-protocols libdecor alsa-lib libpulse \
    qt6-base ffmpeg \
    vulkan-icd-loader vulkan-swrast

git config --global --add safe.directory '*'

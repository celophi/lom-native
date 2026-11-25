#!/bin/sh
set -eu

git submodule update --init external/psyz external/lua
git -C external/psyz submodule update --init external/SDL external/cimgui nugget
git -C external/psyz/external/cimgui submodule update --init imgui

#!/bin/sh
set -eu

cmake --preset opt -G Ninja -DPython3_EXECUTABLE="$(command -v python)"
cmake --build --preset opt

ctest --test-dir build/opt --output-on-failure -LE 'routes|disc'

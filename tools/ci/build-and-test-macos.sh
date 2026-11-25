#!/bin/sh
set -eu

brew install --quiet cmake qt ffmpeg

python3 -m venv .venv
.venv/bin/pip install --quiet -r tools/requirements.txt

cmake --preset opt -DPython3_EXECUTABLE="$PWD/.venv/bin/python" -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build --preset opt

ctest --test-dir build/opt --output-on-failure -LE 'routes|disc'

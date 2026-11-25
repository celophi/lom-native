#!/bin/sh
set -eu

disc=${1:-}

sh tools/ci/install-arch-packages.sh

if [ -n "$disc" ]; then
    cmake --preset opt -DLOM_DISC_ROOT="$disc"
else
    cmake --preset opt
fi
cmake --build --preset opt

if [ -n "$disc" ]; then
    skip=routes
else
    skip='routes|disc'
fi
LOM_TEST_GPU=software ctest --test-dir build/opt --output-on-failure -LE "$skip"

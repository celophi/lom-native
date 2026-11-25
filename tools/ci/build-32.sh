#!/bin/sh
set -eu

sh tools/ci/install-arch-packages.sh

cmake --preset opt32
cmake --build --preset opt32

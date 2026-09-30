#!/bin/sh
# Build touch2tablet from source and install it for the current user (see install.sh).
set -eu
cd "$(dirname "$0")/.."
cmake -S . -B build -G Ninja >/dev/null
cmake --build build
rm -rf build/stage
cmake --install build --prefix build/stage >/dev/null
exec build/stage/install.sh

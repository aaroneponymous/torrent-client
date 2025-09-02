#!/usr/bin/env bash
set -e

BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    mkdir "$BUILD_DIR"
fi

cd "$BUILD_DIR"

cmake ..
cmake --build . -j$(nproc)

if [ "$1" == "test" ]; then
    ctest --output-on-failure
fi

#!/usr/bin/env bash
set -e

# Go to script’s directory (so you can run it from anywhere)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="$SCRIPT_DIR/build"

# Clean build dir if you want fresh builds
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Debug ../

# Build
cmake --build .

echo "Build complete. Run the test with:"
echo "  $BUILD_DIR/metainfo_test <path-to-torrent> [magnet-uri]"

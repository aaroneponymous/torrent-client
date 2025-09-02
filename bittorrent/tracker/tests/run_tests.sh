#!/usr/bin/env bash
set -euo pipefail

# Go to the directory of this script
cd "$(dirname "$0")"

# Create and enter build directory
BUILD_DIR=build
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
cmake ..

# Build with all cores
cmake --build . -j

# Run all test executables explicitly
echo "=== Running test_types ==="
./test_types

echo "=== Running test_compact_peer ==="
./test_compact_peer

echo "=== Running test_endpoint ==="
./test_endpoint

echo "=== Running test_http_tracker ==="
./test_http_tracker

echo "=== Running demo_tracker ==="
./demo_tracker ../sample.torrent

# (Optional) also run ctest to integrate with CTest if desired
# ctest --output-on-failure

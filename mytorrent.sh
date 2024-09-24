set -e

cmake -B build -S .
cmake --build ./build
exec ./build/mytorrent "$@"
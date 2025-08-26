cmake_minimum_required(VERSION 3.16)
project(metainfo_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the include path so we can #include "bencode/bencode.hpp" and "bittorrent/metainfo/metainfo.hpp"
include_directories(${CMAKE_SOURCE_DIR}/include)

# Define the test executable
add_executable(metainfo_test
    metainfo_test.cpp
    ../metainfo.cpp         # Metainfo implementation
    ../../../bencode/bencode.cpp  # Bencode implementation
)

# If sha1.hpp has a corresponding sha1.cpp, add it here. 
# Right now looks header-only, so nothing extra.

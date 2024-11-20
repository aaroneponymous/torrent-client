cmake_minimum_required(VERSION 3.13)

project(mytorrent)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp inlcude/*.hpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

add_executable(bittorrent ${SOURCE_FILES})

target_compile_options(bittorrent PRIVATE 
    -Wall 
    -Wextra 
    -pedantic 
    -Wno-unknown-pragmas 
    -Wno-unused-parameter 
)
cmake_minimum_required(VERSION 3.13)

project(mytorrent)

# Find CURL package
find_package(CURL REQUIRED)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp include/*.hpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

add_executable(bittorrent ${SOURCE_FILES})

# Include CURL headers
target_include_directories(bittorrent PRIVATE ${CURL_INCLUDE_DIRS})

# Link against CURL library
target_link_libraries(bittorrent PRIVATE ${CURL_LIBRARIES})

target_compile_options(bittorrent PRIVATE 
    -Wall 
    -Wextra 
    -pedantic 
    -Wno-unknown-pragmas 
    -Wno-unused-parameter 
)

cmake_minimum_required(VERSION 3.15)
project(bencode LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(bencode STATIC
    bencode.cpp
)

target_include_directories(bencode
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

# Enable testing
include(CTest)

add_executable(bencode_tests tests/test_runner.cpp)
target_link_libraries(bencode_tests PRIVATE bencode)

# Register with CTest
add_test(NAME bencode_basic_tests COMMAND bencode_tests)

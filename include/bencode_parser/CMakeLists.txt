cmake_minimum_required(VERSION 3.13)

project(bencodeParser)

set(CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED True)

add_library(bencode_parser
    bencode_parser.cpp
)

add_executable(unit_test
    unit_test.cpp
)

target_link_libraries(unit_test PRIVATE bencode_parser)

target_compile_options(bencode_parser PRIVATE 
    -Wall 
    -Wextra 
    -pedantic 
    -Wno-unknown-pragmas 
    -Wno-unused-parameter 
)

target_compile_options(unit_test PRIVATE 
    -Wall 
    -Wextra 
    -pedantic 
    -Wno-unknown-pragmas 
    -Wno-unused-parameter 
)

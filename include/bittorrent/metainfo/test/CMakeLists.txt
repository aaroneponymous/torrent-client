cmake_minimum_required(VERSION 3.16)
project(metainfo_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(metainfo_test
    metainfo_test.cpp
    ../metainfo.cpp
    ../../../bencode/bencode.cpp
)

find_package(OpenSSL REQUIRED)
target_link_libraries(metainfo_test PRIVATE OpenSSL::Crypto)


target_include_directories(metainfo_test PRIVATE ${CMAKE_SOURCE_DIR}/.. ${CMAKE_SOURCE_DIR}/../../../)

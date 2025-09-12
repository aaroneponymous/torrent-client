cmake_minimum_required(VERSION 3.16)
project(tracker_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Catch2 3 REQUIRED)  # Catch2::Catch2WithMain

include(CTest)


# ---------------------------------------
# test_logger (tests logger.hpp/cpp)
# ---------------------------------------

add_executable(test_logger
    ../logger.cpp         # adjust path if logger.cpp is elsewhere
    test_logger.cpp
)

target_include_directories(test_logger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_logger PRIVATE Catch2::Catch2WithMain)

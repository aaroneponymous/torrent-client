cmake_minimum_required(VERSION 3.16)
project(tracker_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
add_link_options(-fsanitize=address,undefined)


find_package(Catch2 3 REQUIRED)  # Catch2::Catch2WithMain
find_package(CURL REQUIRED)

# Paths (relative to this tests/ dir)
set(TRACKER_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/../include)
set(BENCODE_DIR     ${CMAKE_CURRENT_LIST_DIR}/../../bencode)

# Collect all bencode .cpps
file(GLOB BENCODE_SOURCES
    ${BENCODE_DIR}/*.cpp
)

# -----------------------------
# test_types (tests types.cpp)
# -----------------------------
add_executable(test_types
    ../src/types.cpp
    test_types.cpp
)
target_include_directories(test_types PRIVATE ${TRACKER_INCLUDE})
target_link_libraries(test_types PRIVATE Catch2::Catch2WithMain)

# ---------------------------------------
# test_compact_peer (tests compact_peer)
# ---------------------------------------
add_executable(test_compact_peer
    ../src/compact_peer.cpp
    test_compact_peer.cpp
)
target_include_directories(test_compact_peer PRIVATE ${TRACKER_INCLUDE})
target_link_libraries(test_compact_peer PRIVATE Catch2::Catch2WithMain)

# ---------------------------------------
# test_endpoint (tests endpoint.cpp)
# ---------------------------------------
add_executable(test_endpoint
    ../src/endpoint.cpp
    test_endpoint.cpp
)
target_include_directories(test_endpoint PRIVATE ${TRACKER_INCLUDE})
target_link_libraries(test_endpoint PRIVATE Catch2::Catch2WithMain)

# ---------------------------------------
# test_http_tracker (tests http_tracker.cpp)
# NOTE: No libcurl here; no http_client_curl.cpp
# ---------------------------------------
add_executable(test_http_tracker
    ../src/http_tracker.cpp
    ../src/compact_peer.cpp
    ../src/endpoint.cpp
    ../src/types.cpp
    ${BENCODE_SOURCES}
    test_http_tracker.cpp
)
target_include_directories(test_http_tracker PRIVATE
    ${TRACKER_INCLUDE}
    ${BENCODE_DIR}
)
target_link_libraries(test_http_tracker PRIVATE Catch2::Catch2WithMain)

# ---------------------------------------
# test_tracker_manager (tests manager.cpp)
# Needs cURL because manager.cpp references makeCurlClient()
# ---------------------------------------
add_executable(test_tracker_manager
    ../src/types.cpp
    ../src/compact_peer.cpp
    ../src/endpoint.cpp
    ../src/http_tracker.cpp
    ../src/udp_tracker.cpp      # <-- add
    ../src/udp_url.cpp          # <-- add
    ../src/http_client_curl.cpp
    ../src/manager.cpp
    ${BENCODE_SOURCES}
    test_tracker_manager.cpp
)
target_include_directories(test_tracker_manager PRIVATE
    ${TRACKER_INCLUDE}
    ${BENCODE_DIR}
)
target_link_libraries(test_tracker_manager PRIVATE
    Catch2::Catch2WithMain
    CURL::libcurl
)


# Metainfo (relative to this tests/ dir)
set(METAINFO_DIR ${CMAKE_CURRENT_LIST_DIR}/../../metainfo)
file(GLOB METAINFO_SOURCES
    ${METAINFO_DIR}/*.cpp
)


# ---------------------------------------
# demo_tracker (demo runner)
# ---------------------------------------
add_executable(demo_tracker
    ../src/types.cpp
    ../src/compact_peer.cpp
    ../src/endpoint.cpp
    ../src/http_tracker.cpp
    ../src/udp_tracker.cpp      # <-- add
    ../src/udp_url.cpp          # <-- add
    ../src/http_client_curl.cpp
    ../src/manager.cpp
    ${BENCODE_SOURCES}
    ${METAINFO_SOURCES}         # <-- metainfo.cpp, etc.
    demo_tracker.cpp
)
target_include_directories(demo_tracker PRIVATE
    ${TRACKER_INCLUDE}
    ${BENCODE_DIR}
    ${METAINFO_DIR}
)
find_package(OpenSSL REQUIRED)
# (Optional but harmless) If not already present at the top:
# find_package(Threads REQUIRED)

target_link_libraries(demo_tracker PRIVATE
    CURL::libcurl
    OpenSSL::Crypto
)

# ---------------------------------------
# test_udp_tracker (POSIX UDP sockets)
# ---------------------------------------
find_package(Threads REQUIRED)  # only once in the file; keep if already present

add_executable(test_udp_tracker
    ../src/types.cpp
    ../src/udp_tracker.cpp
    ../src/udp_url.cpp
    test_udp_tracker.cpp
)
target_include_directories(test_udp_tracker PRIVATE
    ${TRACKER_INCLUDE}
)

# Link either WithMain (no #define CATCH_CONFIG_MAIN in the .cpp)...
target_link_libraries(test_udp_tracker PRIVATE
    Catch2::Catch2WithMain
    Threads::Threads
)

# ...or, if your test file defines CATCH_CONFIG_MAIN, use this instead:
target_link_libraries(test_udp_tracker PRIVATE
    Catch2::Catch2
    Threads::Threads
)




# -----------------------------
# CTest wiring
# -----------------------------
# enable_testing()
# add_test(NAME test_types            COMMAND test_types)
# add_test(NAME test_compact_peer     COMMAND test_compact_peer)
# add_test(NAME test_endpoint         COMMAND test_endpoint)
# add_test(NAME test_http_tracker     COMMAND test_http_tracker)
# add_test(NAME test_tracker_manager  COMMAND test_tracker_manager)
# add_test(NAME test_udp_tracker COMMAND test_udp_tracker)
cmake_minimum_required(VERSION 3.16)
project(Forge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Options ──────────────────────────────────────────────────────────────────
option(FORGE_BUILD_TESTS "Build unit tests" ON)

# ── Core library (all VCS logic & CLI commands) ──────────────────────────────
file(GLOB_RECURSE LIB_SOURCES src/core/*.cpp src/cli/*.cpp)
add_library(forge_core ${LIB_SOURCES})
target_include_directories(forge_core PUBLIC include)

# Filesystem support (GCC < 9 needs explicit linkage)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(forge_core PUBLIC stdc++fs)
endif()

# ── CLI executable ───────────────────────────────────────────────────────────
add_executable(forge src/main.cpp)
target_link_libraries(forge PRIVATE forge_core)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(forge PRIVATE stdc++fs)
endif()

# ── Benchmark executable ─────────────────────────────────────────────────────
add_executable(forge_bench benchmarks/bench_dedup.cpp)
target_link_libraries(forge_bench PRIVATE forge_core)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(forge_bench PRIVATE stdc++fs)
endif()

# ── Tests (Google Test) ──────────────────────────────────────────────────────
if(FORGE_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    FetchContent_MakeAvailable(googletest)
    enable_testing()

    file(GLOB_RECURSE TEST_SOURCES tests/*.cpp)
    add_executable(forge_tests ${TEST_SOURCES})
    target_link_libraries(forge_tests PRIVATE forge_core GTest::gtest_main)
    target_include_directories(forge_tests PRIVATE include)

    include(GoogleTest)
    gtest_discover_tests(forge_tests)
endif()

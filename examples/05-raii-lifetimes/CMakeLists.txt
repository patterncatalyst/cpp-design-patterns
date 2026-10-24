# examples/05-raii-lifetimes — RAII and deterministic lifetimes: a dock
# bay lease at a warehouse loading dock, plus a general-purpose
# scope-exit utility.
#
#   1. DockBayLease  — acquire-in-constructor, release-in-destructor guard
#                       over one dock bay; move-only, survives an
#                       exception thrown mid-batch.
#   2. ScopeGuard     — a general lambda-on-destruction utility, used here
#                       to roll back a partial label count on failure.
#
# Standalone-buildable from this directory (no containers, same local
# tools as the rest of the book):
#   conan install . --output-folder=build/cpp23 --build=missing \
#       -s compiler.cppstd=23 -s build_type=Release
#   cmake --preset cpp23
#   cmake --build --preset cpp23
#   ctest --preset cpp23 --output-on-failure
#
# Also picked up by the repo root's CMakeLists.txt superbuild via
# add_subdirectory, in which case the guard below skips the project()/
# enable_testing() calls the root project already made.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)

    project(cpp_design_patterns_05_raii_lifetimes LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(raii5_core STATIC
    src/dock_bay_registry.cpp
    src/dock_bay_lease.cpp
    src/labeling_station.cpp
)
target_include_directories(raii5_core PUBLIC src)
target_compile_features(raii5_core PUBLIC cxx_std_20)

add_executable(raii5_demo src/demo_main.cpp)
target_link_libraries(raii5_demo PRIVATE raii5_core)

add_executable(raii5_tests
    tests/dock_bay_lease_test.cpp
    tests/scope_guard_test.cpp
    tests/labeling_station_test.cpp
)
target_link_libraries(raii5_tests PRIVATE raii5_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(raii5_tests)

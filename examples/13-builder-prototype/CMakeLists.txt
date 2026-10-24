# examples/13-builder-prototype - Builder, Prototype, and serialization:
# assembling a freight shipment manifest.
#
#   1. Builder:       ManifestBuilder, a fluent step-by-step assembler for a
#                      ShipmentManifest, validating at build()
#   2. Prototype:      PackageTemplate + PackageTemplateRegistry, cloning a
#                      pre-configured package into a fresh independent copy
#   3. Serialization:  a plain text round trip (serialize_manifest /
#                      deserialize_manifest) that reproduces an equal manifest
#
# Target names are prefixed build13_ so they cannot collide with any other
# example's targets in the repo root's superbuild.
#
# Standalone-buildable from this directory (no containers; local clang +
# CMake + Ninja + Conan only, the same tooling every example in this repo
# uses):
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

    project(cpp_design_patterns_13_builder_prototype LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(build13_manifest_core STATIC
    src/manifest_domain.cpp
    src/package_prototype.cpp
    src/manifest_builder.cpp
    src/manifest_serialization.cpp
)
target_include_directories(build13_manifest_core PUBLIC src)
target_compile_features(build13_manifest_core PUBLIC cxx_std_20)

add_executable(build13_manifest_demo src/demo_main.cpp)
target_link_libraries(build13_manifest_demo PRIVATE build13_manifest_core)

add_executable(build13_manifest_tests
    tests/package_prototype_test.cpp
    tests/manifest_builder_test.cpp
    tests/manifest_serialization_test.cpp
)
target_link_libraries(build13_manifest_tests PRIVATE build13_manifest_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(build13_manifest_tests)

# examples/29-refactoring-workflow - Fowler and Feathers taken through one
# inherited shipping component:
#   Stage 1 LegacyEtaEstimator: the inherited code, wall-clock read inline,
#     a six-argument call, carrier dispatch as an if/else chain.
#   Stage 2 SeamedEtaEstimator: Extract Interface + Parameterize
#     Constructor break the clock dependency. Nothing else changes.
#   Stage 3 ModernEtaEstimator: Extract Function, Replace Conditional with
#     Polymorphism, and Introduce Parameter Object apply Fowler's
#     catalogue now that a safety net exists.
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

    project(cpp_design_patterns_29_refactoring_workflow LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(refactor29_core OBJECT
    src/transit_calculations.cpp
    src/legacy_eta_estimator.cpp
    src/seamed_eta_estimator.cpp
    src/carrier_profile.cpp
    src/modern_eta_estimator.cpp
)
target_include_directories(refactor29_core PUBLIC src)
target_compile_features(refactor29_core PUBLIC cxx_std_20)

add_executable(refactor29_demo src/demo_main.cpp)
target_link_libraries(refactor29_demo PRIVATE refactor29_core)

add_executable(refactor29_tests
    tests/legacy_characterization_test.cpp
    tests/dependency_break_test.cpp
    tests/modern_refactor_test.cpp
)
target_link_libraries(refactor29_tests PRIVATE refactor29_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(refactor29_tests)

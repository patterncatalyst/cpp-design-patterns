# examples/30-refactor-or-rewrite - Refactor or rewrite, taught against a
# legacy freight-pricing engine:
#   LEGACY: LegacyPricingEngine, holding an undocumented zone-rate matrix,
#     two undocumented customer contract overrides, and an unexplained
#     round-up-to-the-quarter-dollar step.
#   MODERN: ModernPricingEngine, a bounded rewrite with the same
#     PricingEngine contract, restructured internals, and the same
#     knowledge preserved rather than dropped.
#   SEAM: StranglerPricingRouter, the strangler fig routing seam that runs
#     legacy-only, modern-only, both compared, or a per-customer gradual
#     cutover, all behind the one stable PricingEngine interface.
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

    project(cpp_design_patterns_30_refactor_or_rewrite LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(strangler30_core OBJECT
    src/legacy_pricing_engine.cpp
    src/modern_pricing_engine.cpp
    src/strangler_pricing_router.cpp
)
target_include_directories(strangler30_core PUBLIC src)
target_compile_features(strangler30_core PUBLIC cxx_std_20)

add_executable(strangler30_demo src/demo_main.cpp)
target_link_libraries(strangler30_demo PRIVATE strangler30_core)

add_executable(strangler30_tests
    tests/legacy_pricing_engine_test.cpp
    tests/modern_pricing_engine_test.cpp
    tests/legacy_modern_agreement_test.cpp
    tests/strangler_pricing_router_test.cpp
)
target_include_directories(strangler30_tests PRIVATE tests)
target_link_libraries(strangler30_tests PRIVATE strangler30_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(strangler30_tests)

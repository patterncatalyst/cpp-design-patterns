# examples/21-crtp-policy - CRTP and policy-based design: shipping-rate
# quoting resolved entirely at compile time.
#
# Two idiomatic C++ expressions of static polymorphism, both with zero
# runtime dispatch cost:
#   1. CRTP:                RateModel<Derived> + a reusable Comparable<Derived>
#                            mixin applied across two unrelated types
#   2. Policy-based design:  ShippingCalculator<Rounding, Surcharge>, composed
#                            at compile time from interchangeable policies
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

    project(cpp_design_patterns_21_crtp_policy LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

# Every type in this example is a class template, a concept, or a
# constexpr-friendly ordinary class, so there is no translation unit worth
# compiling separately into a static library: the "library" here is header
# interface only, included directly by the demo and by every test binary.
add_library(crtp21_core INTERFACE)
target_include_directories(crtp21_core INTERFACE src)
target_compile_features(crtp21_core INTERFACE cxx_std_20)

add_executable(crtp21_demo src/demo_main.cpp)
target_link_libraries(crtp21_demo PRIVATE crtp21_core)

add_executable(crtp21_tests
    tests/rate_model_test.cpp
    tests/comparable_mixin_test.cpp
    tests/shipping_calculator_test.cpp
    tests/deducing_this_test.cpp
)
target_link_libraries(crtp21_tests PRIVATE crtp21_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(crtp21_tests)

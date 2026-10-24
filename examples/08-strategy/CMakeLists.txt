# examples/08-strategy — Strategy pattern: shipping-rate estimation.
#
# Three idiomatic C++ expressions of the same Strategy intent, all pricing
# an identical Shipment identically because they share the pure rate math
# in shipping_domain.cpp:
#   1. Runtime polymorphism   — RateStrategy interface + unique_ptr injection
#   2. Value-semantic callable — std::function-based swappable/composable strategy
#   3. Compile-time policy    — concept-constrained template, chosen at compile time
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

    project(cpp_design_patterns_08_strategy LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(shipping_strategy_core STATIC
    src/shipping_domain.cpp
    src/runtime_strategy.cpp
    src/function_strategy.cpp
)
target_include_directories(shipping_strategy_core PUBLIC src)
target_compile_features(shipping_strategy_core PUBLIC cxx_std_20)

add_executable(shipping_strategy_demo src/demo_main.cpp)
target_link_libraries(shipping_strategy_demo PRIVATE shipping_strategy_core)

add_executable(shipping_strategy_tests
    tests/runtime_strategy_test.cpp
    tests/function_strategy_test.cpp
    tests/policy_strategy_test.cpp
)
target_link_libraries(shipping_strategy_tests PRIVATE shipping_strategy_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(shipping_strategy_tests)

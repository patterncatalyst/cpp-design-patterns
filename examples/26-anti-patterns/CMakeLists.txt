# examples/26-anti-patterns - Anti-patterns and pattern misuse, taught
# against a shipment-pricing domain:
#   BEFORE (over-patterned): ShippingRateRegistry (singleton, global
#     mutable state) + RateCalculatorFactoryProvider (singleton holding a
#     one-implementation RateCalculatorFactory, a factory of factories) +
#     ShipmentOperationsManager (a god object that also validates, logs,
#     and formats, and leaks its surcharge flag as call-order-dependent
#     state instead of a value on its own receipt).
#   AFTER (simplified): RateSettings (a plain value) injected once into
#     ShipmentPricingService, returning a PricingResult value that carries
#     everything a caller needs.
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

    project(cpp_design_patterns_26_anti_patterns LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(antipattern26_core OBJECT
    src/shipping_rate_registry.cpp
    src/rate_calculator.cpp
    src/rate_calculator_factory.cpp
    src/shipment_operations_manager.cpp
    src/shipment_pricing_service.cpp
)
target_include_directories(antipattern26_core PUBLIC src)
target_compile_features(antipattern26_core PUBLIC cxx_std_20)

add_executable(antipattern26_demo src/demo_main.cpp)
target_link_libraries(antipattern26_demo PRIVATE antipattern26_core)

add_executable(antipattern26_tests
    tests/equivalence_test.cpp
    tests/shipping_rate_registry_singleton_test.cpp
    tests/shipment_operations_manager_test.cpp
    tests/shipment_pricing_service_test.cpp
)
target_link_libraries(antipattern26_tests PRIVATE antipattern26_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(antipattern26_tests)

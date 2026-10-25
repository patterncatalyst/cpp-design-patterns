# examples/28-dependency-breaking - a representative subset of Feathers'
# 24 dependency-breaking techniques, modernized for C++20/23, taught
# against inherited shipping code:
#   Extract Interface + Parameterize Constructor (shipment_alert_notifier)
#   Extract and Override Call (shipment_eta_scheduler, virtual in C++20,
#     deducing-this in C++23)
#   Encapsulate Global References (legacy_fuel_surcharge)
#   Introduce Instance Delegator (legacy_carrier_zone_lookup)
#   Adapt Parameter (carrier_manifest_file + manifest_source)
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

    project(cpp_design_patterns_28_dependency_breaking LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(breakdep28_core OBJECT
    src/alert_channel.cpp
    src/shipment_alert_notifier.cpp
    src/shipment_eta_scheduler.cpp
    src/legacy_fuel_surcharge.cpp
    src/legacy_carrier_zone_lookup.cpp
    src/carrier_manifest_file.cpp
    src/manifest_source.cpp
)
target_include_directories(breakdep28_core PUBLIC src)
target_compile_features(breakdep28_core PUBLIC cxx_std_20)

add_executable(breakdep28_demo src/demo_main.cpp)
target_link_libraries(breakdep28_demo PRIVATE breakdep28_core)

add_executable(breakdep28_tests
    tests/shipment_alert_notifier_test.cpp
    tests/shipment_eta_scheduler_test.cpp
    tests/shipment_cost_calculator_test.cpp
    tests/shipment_routing_advisor_test.cpp
    tests/warehouse_dispatcher_test.cpp
)
target_link_libraries(breakdep28_tests PRIVATE breakdep28_core GTest::gtest_main GTest::gmock)

include(GoogleTest)
gtest_discover_tests(breakdep28_tests)

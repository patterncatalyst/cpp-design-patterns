# examples/31-final-project - the book's final project, composing five
# patterns into one shipment dispatch console:
#   STRATEGY: RateStrategy, FlatRateStrategy, WeightTieredRateStrategy
#   FACTORY: CarrierFactory, building a CarrierProfile (carrier name, SLA,
#     and the matching RateStrategy) from a CarrierRequest
#   STATE: Shipment's own lifecycle (Created -> PickedUp -> InTransit ->
#     Delivered), enforced by advance() and rejecting illegal events
#   COMMAND: ShipmentCommand, AdvanceLifecycleCommand, AdjustWeightCommand,
#     and the CommandHistory undo stack that runs them
#   OBSERVER: LifecycleObserver, fanned out by Shipment on every legal
#     transition and every undo-driven correction
#   ShipmentDispatcher is the composition root wiring all five together.
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

    project(cpp_design_patterns_31_final_project LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(finalproject31_core OBJECT
    src/shipment_types.cpp
    src/shipment.cpp
    src/flat_rate_strategy.cpp
    src/weight_tiered_rate_strategy.cpp
    src/carrier_factory.cpp
    src/advance_lifecycle_command.cpp
    src/adjust_weight_command.cpp
    src/command_history.cpp
    src/shipment_dispatcher.cpp
)
target_include_directories(finalproject31_core PUBLIC src)
target_compile_features(finalproject31_core PUBLIC cxx_std_20)

add_executable(finalproject31_demo src/demo_main.cpp)
target_link_libraries(finalproject31_demo PRIVATE finalproject31_core)

add_executable(finalproject31_tests
    tests/shipment_lifecycle_test.cpp
    tests/rate_strategy_test.cpp
    tests/carrier_factory_test.cpp
    tests/adjust_weight_command_test.cpp
    tests/advance_lifecycle_command_test.cpp
    tests/command_history_test.cpp
    tests/shipment_dispatcher_integration_test.cpp
)
target_include_directories(finalproject31_tests PRIVATE tests)
target_link_libraries(finalproject31_tests PRIVATE finalproject31_core GTest::gtest_main GTest::gmock)

include(GoogleTest)
gtest_discover_tests(finalproject31_tests)

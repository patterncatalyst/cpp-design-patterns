# examples/10-state-machines - State pattern + finite state machines: a
# shipment lifecycle.
#
# Four ways to model the same shipment lifecycle (Created -> Paid ->
# InTransit -> Delivered, with Cancelled reachable from Created or Paid),
# each sharing only the ShipmentStatus/ShipmentEvent vocabulary in
# shipment_domain.cpp:
#   1. GoF State pattern: one type per status            (gof_state.hpp)
#   2a. Data-driven FSM:  enum + transition table         (table_fsm.hpp)
#   2b. Variant FSM:      std::variant of tags + std::visit (variant_fsm.hpp)
#   3. Stochastic FSM:    weighted, RNG-injected transitions (stochastic_fsm.hpp)
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
# enable_testing() calls the root project already made. Every target name
# is prefixed state10_ so it never collides with another example's targets
# in the superbuild.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)

    project(cpp_design_patterns_10_state_machines LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(state10_core STATIC
    src/shipment_domain.cpp
    src/gof_state.cpp
    src/table_fsm.cpp
    src/variant_fsm.cpp
    src/stochastic_fsm.cpp
)
target_include_directories(state10_core PUBLIC src)
target_compile_features(state10_core PUBLIC cxx_std_20)

add_executable(state10_demo src/demo_main.cpp)
target_link_libraries(state10_demo PRIVATE state10_core)

add_executable(state10_tests
    tests/gof_state_test.cpp
    tests/table_fsm_test.cpp
    tests/variant_fsm_test.cpp
    tests/stochastic_fsm_test.cpp
    tests/cross_check_test.cpp
)
target_link_libraries(state10_tests PRIVATE state10_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(state10_tests)

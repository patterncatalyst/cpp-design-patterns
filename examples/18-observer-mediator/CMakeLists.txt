# examples/18-observer-mediator - Observer and Mediator: a shipment status
# tracker and a warehouse dispatch coordinator.
#
# Observer (shipment_observer.hpp/.cpp): ShipmentTracker is the subject.
# DashboardView and AlertingService are interface-based observers; a plain
# std::function callback shows the lighter, class-free form of the same
# subscription.
#
# Mediator (dispatch_mediator.hpp/.cpp): DispatchCoordinator mediates
# between WarehouseStation, CarrierStation, and CustomerNotificationStation,
# each of which only ever talks to its mediator, never to another colleague
# directly.
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

    project(cpp_design_patterns_18_observer_mediator LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(observe18_core STATIC
    src/shipment_observer.cpp
    src/dispatch_mediator.cpp
)
target_include_directories(observe18_core PUBLIC src)
target_compile_features(observe18_core PUBLIC cxx_std_20)

add_executable(observe18_demo src/demo_main.cpp)
target_link_libraries(observe18_demo PRIVATE observe18_core)

add_executable(observe18_tests
    tests/shipment_observer_test.cpp
    tests/dispatch_mediator_test.cpp
)
target_link_libraries(observe18_tests PRIVATE observe18_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(observe18_tests)

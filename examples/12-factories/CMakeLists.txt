# examples/12-factories - Factory Method and Abstract Factory: carrier
# integration.
#
# Two idiomatic C++ expressions, taught side by side in one carrier
# integration domain:
#   1. Abstract Factory: CarrierIntegrationFactory produces a matching
#      family of three products (LabelFormatter, RateQuoter,
#      TrackingClient) per carrier, so a label formatter from one carrier
#      can never end up paired with a rate quoter from another.
#   2. Factory Method:   CarrierOnboarder's fixed onboarding sequence calls
#      a private virtual create_test_suite() that each environment
#      subclass overrides to pick its own IntegrationTestSuite.
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

    project(cpp_design_patterns_12_factories LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(factory12_core STATIC
    src/abstract_factory.cpp
    src/factory_method.cpp
)
target_include_directories(factory12_core PUBLIC src)
target_compile_features(factory12_core PUBLIC cxx_std_20)

add_executable(factory12_demo src/demo_main.cpp)
target_link_libraries(factory12_demo PRIVATE factory12_core)

add_executable(factory12_tests
    tests/abstract_factory_test.cpp
    tests/factory_method_test.cpp
)
target_link_libraries(factory12_tests PRIVATE factory12_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(factory12_tests)

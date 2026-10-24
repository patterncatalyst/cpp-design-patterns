# examples/15-adapter-facade-bridge - Adapter, Facade, Bridge, and PImpl:
# a shipping subsystem.
#
# Four idiomatic C++ techniques, taught in one cohesive shipping domain:
#   1. Adapter:  LegacyFreightRateAdapter wraps the legacy GlobalFreight
#      rate API (awkward units, awkward names) behind the book's own
#      RateSource interface, by composition (object adapter).
#   2. Facade:   ShippingFacade's quote_and_label() fronts three
#      subsystems (rating, labeling, tracking) with one call.
#   3. Bridge:   Notification (with refined kinds) varies independently of
#      NotificationTransport (its delivery backends).
#   4. PImpl:    LabelRenderer hides its formatting details behind a
#      std::unique_ptr<Impl> compilation firewall, and still copies and
#      moves correctly.
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

    project(cpp_design_patterns_15_adapter_facade_bridge LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(adapt15_core STATIC
    src/legacy_rate_api.cpp
    src/rate_adapter.cpp
    src/label_renderer.cpp
    src/shipping_facade.cpp
    src/notification.cpp
)
target_include_directories(adapt15_core PUBLIC src)
target_compile_features(adapt15_core PUBLIC cxx_std_20)

add_executable(adapt15_demo src/demo_main.cpp)
target_link_libraries(adapt15_demo PRIVATE adapt15_core)

add_executable(adapt15_tests
    tests/rate_adapter_test.cpp
    tests/label_renderer_test.cpp
    tests/shipping_facade_test.cpp
    tests/notification_test.cpp
)
target_link_libraries(adapt15_tests PRIVATE adapt15_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(adapt15_tests)

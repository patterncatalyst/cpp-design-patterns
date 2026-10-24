# examples/17-composite-flyweight - Composite, Flyweight, and Null Object:
# palletizing a shipment.
#
# Three idioms taught in one domain:
#   1. Composite:   PackageComponent is the uniform interface Item (leaf)
#                   and PackageGroup (composite: a carton or a pallet) both
#                   implement, so total_weight_grams() / item_count() recurse
#                   the same way over a mixed, arbitrarily nested tree.
#   2. Flyweight:   SkuCatalog hands out a shared, immutable SkuDescriptor
#                   per SKU code; many Items reference a few descriptors.
#   3. Null Object: NullShipmentExceptionHandler is a do-nothing
#                   ShipmentExceptionHandler, the default PackageGroup binds
#                   to, so a weight-limit check never branches on "is there a
#                   handler".
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

    project(cpp_design_patterns_17_composite_flyweight LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(comp17_core STATIC
    src/sku_catalog.cpp
    src/exception_handler.cpp
    src/package_tree.cpp
)
target_include_directories(comp17_core PUBLIC src)
target_compile_features(comp17_core PUBLIC cxx_std_20)

add_executable(comp17_demo src/demo_main.cpp)
target_link_libraries(comp17_demo PRIVATE comp17_core)

add_executable(comp17_tests
    tests/sku_catalog_test.cpp
    tests/exception_handler_test.cpp
    tests/package_tree_test.cpp
)
target_link_libraries(comp17_tests PRIVATE comp17_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(comp17_tests)

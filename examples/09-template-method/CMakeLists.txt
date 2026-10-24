# examples/09-template-method - Template Method + NVI: order export pipeline.
#
# One fixed skeleton, export_order(), validates an order, then calls three
# private virtual hooks in a fixed sequence (header, one line per item,
# footer) before appending an invariant trailer, also not a hook. Three
# concrete exporters override the hooks for a different document format:
#   1. CsvManifestExporter    - comma-separated manifest
#   2. JsonManifestExporter   - newline-delimited JSON
#   3. FixedWidthEdiExporter  - fixed-width EDI-style segments
# A second, narrower pair of classes in static_dispatch_variant.hpp
# contrasts C++20 CRTP against C++23 deducing this for a single hook
# resolved at compile time instead of through a vtable.
#
# Target names are prefixed tmpl9_ so this example's targets never collide
# with another example's targets in the repo root's superbuild.
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

    project(cpp_design_patterns_09_template_method LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(tmpl9_core STATIC
    src/order_domain.cpp
    src/export_pipeline.cpp
)
target_include_directories(tmpl9_core PUBLIC src)
target_compile_features(tmpl9_core PUBLIC cxx_std_20)

add_executable(tmpl9_demo src/demo_main.cpp)
target_link_libraries(tmpl9_demo PRIVATE tmpl9_core)

add_executable(tmpl9_tests
    tests/export_pipeline_test.cpp
    tests/static_dispatch_variant_test.cpp
)
target_link_libraries(tmpl9_tests PRIVATE tmpl9_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(tmpl9_tests)

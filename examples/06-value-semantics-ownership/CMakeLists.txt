# examples/06-value-semantics-ownership - Value semantics, ownership, and
# smart pointers: a warehouse dispatch system.
#
# Four ownership shapes, each a contrast between pointer kinds rather than
# a syntax demo:
#   1. Parcel:         a value type following the Rule of Zero; a copy is
#                       independent of the original.
#   2. DockSlot:        sole ownership of a Parcel via unique_ptr, with
#                       ownership transferred between slots by move.
#   3. CarrierContract: a resource-owning type following the Rule of
#                       Five, shared by more than one ShippingRoute via
#                       shared_ptr (shared reference semantics).
#   4. ManifestNode:    a parent/child object graph where the parent
#                       owns children via shared_ptr and a child
#                       observes its parent via weak_ptr, breaking what
#                       would otherwise be a reference cycle.
# Plus a C++23 refinement: adopting a raw pointer from a C-style legacy
# factory function into a unique_ptr with std::out_ptr, contrasted with the
# manual C++20 way.
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

    project(cpp_design_patterns_06_value_semantics_ownership LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(value6_ownership_core STATIC
    src/dock_slot.cpp
    src/carrier_contract.cpp
    src/shipping_route.cpp
    src/manifest_tree.cpp
    src/legacy_rate_table.cpp
    src/rate_table_adapter.cpp
)
target_include_directories(value6_ownership_core PUBLIC src)
target_compile_features(value6_ownership_core PUBLIC cxx_std_20)

add_executable(value6_ownership_demo src/demo_main.cpp)
target_link_libraries(value6_ownership_demo PRIVATE value6_ownership_core)

add_executable(value6_ownership_tests
    tests/parcel_value_semantics_test.cpp
    tests/dock_slot_test.cpp
    tests/carrier_contract_test.cpp
    tests/manifest_tree_test.cpp
    tests/rate_table_adapter_test.cpp
)
target_link_libraries(value6_ownership_tests PRIVATE value6_ownership_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(value6_ownership_tests)

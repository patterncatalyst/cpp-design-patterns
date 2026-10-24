# examples/19-chain-iterator - Chain of Responsibility and Iterator: a
# shipment-approval chain and a zone-bucketed package manifest.
#
# Chain of Responsibility (approval_chain.hpp/.cpp): ApprovalHandler is the
# shared base. WeightLimitHandler, HazmatHandler, and EmbargoHandler each
# either reject a ShipmentRequest or pass it to the next handler;
# ManualApprovalHandler is the fallback at the end of the chain and always
# reaches a decision.
#
# Iterator (package_manifest.hpp/.cpp): PackageManifest stores Package
# values bucketed by DestinationZone. const_iterator is a hand-written
# forward iterator that flattens the buckets into zone-ordered traversal,
# and the same manifest is also a std::ranges::forward_range, so a
# std::views::filter + std::views::transform pipeline runs over it directly.
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

    project(cpp_design_patterns_19_chain_iterator LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(chain19_core STATIC
    src/approval_chain.cpp
    src/package_manifest.cpp
)
target_include_directories(chain19_core PUBLIC src)
target_compile_features(chain19_core PUBLIC cxx_std_20)

add_executable(chain19_demo src/demo_main.cpp)
target_link_libraries(chain19_demo PRIVATE chain19_core)

add_executable(chain19_tests
    tests/approval_chain_test.cpp
    tests/package_manifest_test.cpp
)
target_link_libraries(chain19_tests PRIVATE chain19_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(chain19_tests)

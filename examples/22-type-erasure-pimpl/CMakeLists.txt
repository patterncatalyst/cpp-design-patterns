# examples/22-type-erasure-pimpl - Type erasure and PImpl: a quoting
# ledger built from unrelated concrete types.
#
# Two idiomatic C++ techniques built on the same shipping domain:
#   1. Type erasure: AnyRateQuoter holds ANY type exposing a
#      quote(const Shipment&) -> Cents member, with no shared base class,
#      dispatching through a hand-written concept/model pair (the same
#      technique std::function and std::any use internally).
#   2. PImpl:        RateQuoteLedger hides its bookkeeping behind a
#      std::unique_ptr<Impl> compilation firewall, and still copies deeply
#      and moves correctly.
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

    project(cpp_design_patterns_22_type_erasure_pimpl LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(erase22_core STATIC
    src/rate_quote_ledger.cpp
)
target_include_directories(erase22_core PUBLIC src)
target_compile_features(erase22_core PUBLIC cxx_std_20)

add_executable(erase22_demo src/demo_main.cpp)
target_link_libraries(erase22_demo PRIVATE erase22_core)

add_executable(erase22_tests
    tests/any_rate_quoter_test.cpp
    tests/rate_quote_ledger_test.cpp
)
target_link_libraries(erase22_tests PRIVATE erase22_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(erase22_tests)

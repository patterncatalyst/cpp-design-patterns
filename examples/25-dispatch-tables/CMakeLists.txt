# examples/25-dispatch-tables - Three lightweight dispatch mechanisms,
# taught against a shipment-tracking ingestion service:
#   1. Jump table:        an enum-indexed array of raw MessageHandler
#                          function pointers, the closed-set, fastest form.
#   2. Dispatch map:       std::unordered_map<string, Handler> for an open,
#                          extensible set of carrier wire tags, registered
#                          at runtime (plus a std::flat_map variant under
#                          C++23).
#   3. constexpr table:    std::array<MessageHandler, N> built and checked
#                          complete at compile time.
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

    project(cpp_design_patterns_25_dispatch_tables LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

# Built as an OBJECT library, not STATIC, for the same reason Chapter 23's
# self-registering registry needed one: nothing outside this build links
# every translation unit here by name, and an OBJECT library guarantees
# every .o file passes straight through to whatever links it rather than
# being dropped as "unused" by a linker looking at a STATIC archive. This
# example doesn't self-register at static-init time the way Chapter 23's
# carrier policies do, but keeping the same OBJECT shape costs nothing and
# keeps every example in this book free of that footgun by default.
add_library(dispatch25_core OBJECT
    src/tracking_ledger.cpp
    src/message_handlers.cpp
    src/jump_table.cpp
    src/dispatch_table_constexpr.cpp
    src/dispatch_map.cpp
)
target_include_directories(dispatch25_core PUBLIC src)
target_compile_features(dispatch25_core PUBLIC cxx_std_20)

add_executable(dispatch25_demo src/demo_main.cpp)
target_link_libraries(dispatch25_demo PRIVATE dispatch25_core)

add_executable(dispatch25_tests
    tests/jump_table_test.cpp
    tests/constexpr_table_test.cpp
    tests/dispatch_map_test.cpp
)
target_include_directories(dispatch25_tests PRIVATE tests)
target_link_libraries(dispatch25_tests PRIVATE dispatch25_core GTest::gtest_main GTest::gmock)

include(GoogleTest)
gtest_discover_tests(dispatch25_tests)

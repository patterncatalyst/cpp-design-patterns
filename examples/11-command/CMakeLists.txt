# examples/11-command - Command pattern: warehouse inventory and order
# editing with undo/redo and a command queue.
#
# Four InventoryCommand objects (AddStockCommand, MoveItemCommand,
# AdjustQuantityCommand, CancelLineCommand) share one receiver,
# WarehouseEditor, and one CommandHistory undo/redo stack. A CommandQueue
# defers and batches commands in FIFO order. A separate std::function (or
# C++23 std::move_only_function) shortcut, RestockLogFn, shows the
# idiomatic alternative for a queued action with nothing to undo.
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

    project(cpp_design_patterns_11_command LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(cmd11_warehouse_core STATIC
    src/warehouse_domain.cpp
    src/command.cpp
    src/command_history.cpp
    src/command_queue.cpp
    src/restock_log.cpp
)
target_include_directories(cmd11_warehouse_core PUBLIC src)
target_compile_features(cmd11_warehouse_core PUBLIC cxx_std_20)

add_executable(cmd11_warehouse_demo src/demo_main.cpp)
target_link_libraries(cmd11_warehouse_demo PRIVATE cmd11_warehouse_core)

add_executable(cmd11_warehouse_tests
    tests/command_test.cpp
    tests/command_history_test.cpp
    tests/command_queue_test.cpp
    tests/restock_log_test.cpp
)
target_link_libraries(cmd11_warehouse_tests PRIVATE cmd11_warehouse_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(cmd11_warehouse_tests)

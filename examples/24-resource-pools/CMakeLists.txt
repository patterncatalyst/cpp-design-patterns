# examples/24-resource-pools - the generic ResourcePool<Resource> + PoolLease
# RAII handle, plus four patterns built on or alongside it, taught against a
# shipment-tracking domain:
#   1. ResourcePool<Resource> + Lease:  src/resource_pool.hpp (centerpiece)
#   2. Connection pool (canonical use): TrackingStoreConnectionPool
#   3. Thread pool:                     LabelRenderThreadPool
#   4. Monitor object:                  ShipmentManifestMonitor
#   5. Active object:                   LabelPrinterActiveObject
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

    project(cpp_design_patterns_24_resource_pools LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# OBJECT, not STATIC, for consistency with this repo's library convention
# (see ch23's CMakeLists.txt for the full self-registration rationale).
# Nothing in this example self-registers; OBJECT costs nothing here and
# keeps every example in the book built the same way.
add_library(pool24_core OBJECT
    src/tracking_store_connection.cpp
    src/connection_factory.cpp
    src/shipping_label.cpp
    src/label_render_thread_pool.cpp
    src/label_printer_active_object.cpp
)
target_include_directories(pool24_core PUBLIC src)
target_compile_features(pool24_core PUBLIC cxx_std_20)
target_link_libraries(pool24_core PUBLIC Threads::Threads)

add_executable(pool24_demo src/demo_main.cpp)
target_link_libraries(pool24_demo PRIVATE pool24_core)

add_executable(pool24_tests
    tests/resource_pool_test.cpp
    tests/tracking_store_connection_pool_test.cpp
    tests/label_render_thread_pool_test.cpp
    tests/label_printer_active_object_test.cpp
    tests/shipment_manifest_monitor_test.cpp
)
target_link_libraries(pool24_tests PRIVATE
    pool24_core
    GTest::gtest_main
    GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(pool24_tests)

# examples/14-singleton-and-di - Singleton, and the dependency-injection
# alternative: auditing a shipment fulfillment event.
#
# Two ways to model the same capability, an append-only audit log for
# order-fulfillment events:
#   1. ShipmentAuditLog: a thread-safe Meyers singleton, reached from
#      anywhere through instance(), with no constructor parameter
#      announcing the dependency.
#   2. AuditSink + OrderFulfillmentService: the same capability as an
#      interface, injected by constructor, wired once by a composition
#      root (demo_main.cpp), swappable with a test double in a unit test.
#
# Target names are prefixed single14_ so this example's targets never
# collide with another example's targets in the repo root's superbuild.
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

    project(cpp_design_patterns_14_singleton_and_di LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(single14_audit_core STATIC
    src/audit_domain.cpp
    src/shipment_audit_log_singleton.cpp
    src/audit_sink.cpp
    src/order_fulfillment_service.cpp
)
target_include_directories(single14_audit_core PUBLIC src)
target_compile_features(single14_audit_core PUBLIC cxx_std_20)

add_executable(single14_audit_demo src/demo_main.cpp)
target_link_libraries(single14_audit_demo PRIVATE single14_audit_core)

add_executable(single14_audit_tests
    tests/shipment_audit_log_singleton_test.cpp
    tests/order_fulfillment_service_test.cpp
)
target_include_directories(single14_audit_tests PRIVATE tests)
target_link_libraries(single14_audit_tests PRIVATE single14_audit_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(single14_audit_tests)

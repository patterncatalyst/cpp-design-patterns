# examples/27-seams - Characterization tests and the three seam techniques
# (object, link, preprocessor), taught against an inherited shipment-SLA
# checker and billing routine:
#   BEFORE: LegacyShipmentSlaChecker reads std::chrono::system_clock::now()
#     directly from inside is_breached(), with no seam of any kind.
#   AFTER (object seam, the centerpiece): ShipmentSlaChecker takes a Clock
#     reference through its constructor, so a test can substitute a
#     MockClock (GoogleMock) in place of SystemClock. Also carries a
#     deliberate behavior change (the exact-deadline boundary) made safely
#     once the seam exists.
#   Link seam: late_fee_cents() is a bare free function, with no object to
#     inject anything into, that calls grace_period(). grace_period() is
#     declared once in grace_policy.hpp and defined twice: once in
#     grace_policy_production.cpp (compiled into seam27_demo) and once in
#     tests/grace_policy_test_double.cpp (compiled into seam27_tests).
#     late_fee_schedule.cpp, compiled once into seam27_core, never changes
#     between the two; only the linker's choice of object file does.
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

    project(cpp_design_patterns_27_seams LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

# OBJECT, not STATIC, for consistency with this repo's library convention.
# grace_policy_production.cpp and tests/grace_policy_test_double.cpp are
# deliberately NOT part of this library: each one is compiled directly
# into exactly one of the two executables below, which is the whole
# mechanism the link seam demonstrates.
add_library(seam27_core OBJECT
    src/clock.cpp
    src/legacy_shipment_sla_checker.cpp
    src/shipment_sla_checker.cpp
    src/late_fee_schedule.cpp
)
target_include_directories(seam27_core PUBLIC src)
target_compile_features(seam27_core PUBLIC cxx_std_20)

add_executable(seam27_demo
    src/demo_main.cpp
    src/grace_policy_production.cpp
)
target_link_libraries(seam27_demo PRIVATE seam27_core)

add_executable(seam27_tests
    tests/legacy_shipment_sla_checker_characterization_test.cpp
    tests/shipment_sla_checker_test.cpp
    tests/late_fee_schedule_test.cpp
    tests/grace_policy_test_double.cpp
)
target_link_libraries(seam27_tests PRIVATE
    seam27_core
    GTest::gtest_main
    GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(seam27_tests)

# examples/23-expression-templates - Expression templates plus two
# supporting idioms, taught against a multi-leg freight route domain:
#   1. Expression templates:          RouteMetrics + RouteSum/RouteScaled
#                                      build a lazy expression tree that
#                                      evaluates in one pass, no temporaries.
#   2. Self-registering registry:     CarrierPolicy + CarrierPolicyRegistrar,
#                                      each concrete carrier registers itself
#                                      from its own translation unit.
#   3. Templated serialization:       CarrierProfile + a single
#                                      describe_fields() hook drives both
#                                      serialize() and deserialize().
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

    project(cpp_design_patterns_23_expression_templates LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

# Built as an OBJECT library, not STATIC. The self-registering carrier
# policies (flat_rate_carrier_policy.cpp and friends) are referenced by
# NOTHING outside their own translation unit except the side effect their
# registrar runs during static initialization; no other code calls a symbol
# any of them define. A STATIC archive only pulls in the .o files a linker
# can see a live symbol reference to, so a linker is free to drop one of
# those translation units, registrar and all, as "unused." An OBJECT
# library passes every .o file straight through to whatever links it, so
# nothing a self-registering idiom depends on can be silently dropped.
add_library(exprtmpl23_core OBJECT
    src/route_metrics.cpp
    src/eager_route_metrics.cpp
    src/carrier_policy.cpp
    src/flat_rate_carrier_policy.cpp
    src/weight_tiered_carrier_policy.cpp
    src/regional_express_carrier_policy.cpp
    src/carrier_profile_serialization.cpp
)
target_include_directories(exprtmpl23_core PUBLIC src)
target_compile_features(exprtmpl23_core PUBLIC cxx_std_20)

add_executable(exprtmpl23_demo src/demo_main.cpp)
target_link_libraries(exprtmpl23_demo PRIVATE exprtmpl23_core)

add_executable(exprtmpl23_tests
    tests/expression_template_test.cpp
    tests/carrier_policy_registry_test.cpp
    tests/carrier_profile_serialization_test.cpp
)
target_link_libraries(exprtmpl23_tests PRIVATE exprtmpl23_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(exprtmpl23_tests)

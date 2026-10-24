# examples/07-testing-with-gtest: proving QuoteIntake with GoogleTest +
# GoogleMock.
#
# A freight quote intake component, exercised through the full range of
# GoogleTest styles this chapter teaches:
#   - TEST_P + INSTANTIATE_TEST_SUITE_P over a table of ShipmentRequest ->
#     ValidationOutcome cases (quote_domain_test.cpp)
#   - EXPECT_DEATH for a violated precondition (quote_domain_test.cpp)
#   - TYPED_TEST_SUITE proving the same QuoteIdGenerator contract against
#     two unrelated concrete generators (quote_id_generator_test.cpp)
#   - TEST_F with SetUp/TearDown sharing a mock ledger, a real id
#     generator, and the intake under test, plus GoogleMock EXPECT_CALL
#     with matchers and actions on the ledger collaborator
#     (quote_intake_test.cpp)
#
# Target names are prefixed testing7_ so this example's targets never
# collide with another example's targets in the repo root's superbuild.
#
# Standalone-buildable from this directory (no containers, same local
# tools every example in this book uses):
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

    project(cpp_design_patterns_07_testing_with_gtest LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(testing7_quote_core STATIC
    src/quote_domain.cpp
    src/quote_id_generator.cpp
    src/quote_intake.cpp
)
target_include_directories(testing7_quote_core PUBLIC src)
target_compile_features(testing7_quote_core PUBLIC cxx_std_20)

add_executable(testing7_quote_demo src/demo_main.cpp)
target_link_libraries(testing7_quote_demo PRIVATE testing7_quote_core)

add_executable(testing7_quote_tests
    tests/quote_domain_test.cpp
    tests/quote_id_generator_test.cpp
    tests/quote_intake_test.cpp
)
target_include_directories(testing7_quote_tests PRIVATE tests)
target_link_libraries(testing7_quote_tests PRIVATE
    testing7_quote_core
    GTest::gtest_main
    GTest::gmock
)

include(GoogleTest)
# NO_PRETTY_VALUES: the CMake >= 3.28 floor this book pins (verified against
# 4.3.0, see scripts/check-prereqs.sh) rewrites a value-parameterized test's
# CTest name using GetParam()'s raw printed form whenever one exists, even
# when INSTANTIATE_TEST_SUITE_P already gave it a readable name via a name
# generator, which is exactly what ShipmentValidationTest below does. Later
# CMake releases only do that rewrite for googletest's own numeric default
# names. NO_PRETTY_VALUES opts out of the rewrite entirely so the custom
# names INSTANTIATE_TEST_SUITE_P assigns reach `ctest -N` unmodified instead
# of as an unreadable byte dump.
gtest_discover_tests(testing7_quote_tests NO_PRETTY_VALUES)

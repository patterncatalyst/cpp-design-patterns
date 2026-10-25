# examples/32-ai-and-llm-tools - Working with AI and LLM tools, taught
# against a metered API gateway's usage-tier Strategy:
#   CorrectUsageTierStrategy: the pricing spec's actual boundaries
#     (Starter/Growth/Scale), both lower bounds inclusive.
#   PlausibleWrongUsageTierStrategy: the AI-plausible, subtly wrong shape
#     an LLM pairing partner handed back for the identical prompt, one
#     comparison operator off at the Starter/Growth boundary.
#   The behavior table in tests/usage_tier_classification_test.cpp is the
#   verification gate: it passes against the correct implementation and
#   demonstrably fails, via EXPECT_NONFATAL_FAILURE, against the wrong one.
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

    project(cpp_design_patterns_32_ai_and_llm_tools LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(aicollab32_core OBJECT
    src/usage_tier.cpp
    src/correct_usage_tier_strategy.cpp
    src/plausible_wrong_usage_tier_strategy.cpp
    src/billing.cpp
)
target_include_directories(aicollab32_core PUBLIC src)
target_compile_features(aicollab32_core PUBLIC cxx_std_20)

add_executable(aicollab32_demo src/demo_main.cpp)
target_link_libraries(aicollab32_demo PRIVATE aicollab32_core)

add_executable(aicollab32_tests
    tests/usage_tier_classification_test.cpp
    tests/billing_test.cpp
)
target_link_libraries(aicollab32_tests PRIVATE aicollab32_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(aicollab32_tests)

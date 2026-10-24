# examples/03-solid-first-principles — SRP, OCP, LSP: billing an invoice.
#
# Three SOLID principles made concrete against one small billing domain:
#   SRP — InvoiceCalculator / InvoiceFormatter / InMemoryInvoiceRepository
#         each have exactly one reason to change.
#   OCP — InvoiceRule is the extension point; new rule types (and the test
#         suite's own RoundUpToWholeDollarRule) join InvoiceCalculator
#         without editing it.
#   LSP — LoyaltyDiscountRule is substitutable anywhere a
#         PercentageDiscountRule or an InvoiceRule is expected.
#
# Standalone-buildable from this directory (no containers, same local
# tools as the rest of the book):
#   conan install . --output-folder=build/cpp23 --build=missing \
#       -s compiler.cppstd=23 -s build_type=Release
#   cmake --preset cpp23
#   cmake --build --preset cpp23
#   ctest --preset cpp23 --output-on-failure
#
# Target names are prefixed solid1_ so this example's targets never
# collide with another example's targets (notably solid2_* from
# examples/04-solid-dependency-inversion) in the repo root's superbuild.
#
# Also picked up by the repo root's CMakeLists.txt superbuild via
# add_subdirectory, in which case the guard below skips the project()/
# enable_testing() calls the root project already made.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)

    project(cpp_design_patterns_03_solid_first_principles LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(solid1_billing_core STATIC
    src/billing_domain.cpp
    src/invoice_rules.cpp
    src/invoice_calculator.cpp
    src/invoice_formatter.cpp
    src/invoice_repository.cpp
)
target_include_directories(solid1_billing_core PUBLIC src)
target_compile_features(solid1_billing_core PUBLIC cxx_std_20)

add_executable(solid1_billing_demo src/demo_main.cpp)
target_link_libraries(solid1_billing_demo PRIVATE solid1_billing_core)

add_executable(solid1_billing_tests
    tests/invoice_calculator_test.cpp
    tests/invoice_formatter_test.cpp
    tests/invoice_repository_test.cpp
    tests/invoice_lsp_test.cpp
)
target_link_libraries(solid1_billing_tests PRIVATE solid1_billing_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(solid1_billing_tests)

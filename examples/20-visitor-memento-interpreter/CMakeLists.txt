# examples/20-visitor-memento-interpreter - Visitor, Memento, and Interpreter:
# a shipping-filter rule AST evaluated two ways, plus a whole-state snapshot.
#
# - Visitor (classic, src/classic_visitor.hpp/.cpp): a RuleNode class
#   hierarchy + RuleVisitor interface, GoF double dispatch via accept()/
#   visit(). Two operations, MatchEvaluator and RuleDescriber, ship with
#   zero changes to any RuleNode subclass.
# - Visitor (modern, src/rule_expr.hpp/.cpp): the same six node kinds as a
#   std::variant of value types, dispatched with std::visit and an
#   Overload set instead of a vtable.
# - Interpreter (src/rule_expr.hpp/.cpp, evaluate()): walks the same AST
#   to evaluate a small filter grammar; nesting of And/Or/Not changes the
#   result the way operator precedence would in a parsed expression.
# - Memento (src/rule_set_editor.hpp/.cpp): RuleSetEditor is the
#   Originator, Snapshot is the opaque Memento, save()/restore() capture
#   and replace the editor's whole state in one call.
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

    project(cpp_design_patterns_20_visitor_memento_interpreter LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(visit20_core STATIC
    src/shipping_domain.cpp
    src/classic_visitor.cpp
    src/rule_expr.cpp
    src/rule_set_editor.cpp
)
target_include_directories(visit20_core PUBLIC src)
target_compile_features(visit20_core PUBLIC cxx_std_20)

add_executable(visit20_demo src/demo_main.cpp)
target_link_libraries(visit20_demo PRIVATE visit20_core)

add_executable(visit20_tests
    tests/classic_visitor_test.cpp
    tests/rule_expr_test.cpp
    tests/rule_set_editor_test.cpp
)
target_link_libraries(visit20_tests PRIVATE visit20_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(visit20_tests)

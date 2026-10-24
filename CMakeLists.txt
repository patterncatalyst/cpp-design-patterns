# Root superbuild for Design Patterns with C++.
#
# This configures and builds every example under examples/ in one pass for
# local development and CI. Each example directory is ALSO standalone-
# buildable on its own: `cd examples/<NN-pattern> && conan install . &&
# cmake --preset cpp23 && cmake --build --preset cpp23` works without ever
# touching this file. That property matters for the book — a reader who
# only cares about one pattern should not have to configure the whole repo.
#
# No containers: this project builds with the host's clang, CMake, Ninja,
# and Conan (see scripts/check-prereqs.sh and _docs/00-prerequisites.md for
# the pinned versions).

cmake_minimum_required(VERSION 3.28)

project(cpp_design_patterns LANGUAGES CXX)

# Default standard for the superbuild and for any example that doesn't set
# its own CMAKE_CXX_STANDARD, used only when nothing upstream has already
# chosen one (a bare `cmake -S . -B build` with no preset). The cpp23/cpp20
# configure presets in CMakePresets.json (and the Conan toolchain file each
# one loads) set CMAKE_CXX_STANDARD as a cache variable before this file
# runs, so guard the default — an unconditional set() here would shadow
# that cache variable and silently force every preset onto C++23,
# including the cpp20 compatibility preset. C++20 and C++23 are the only
# two standards this book covers; C++26 is deferred — see Appendix A.
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

# Discover every example's CMakeLists.txt and pull it into the superbuild.
# This must configure cleanly with ZERO example directories present, which
# is the state right after r01's scaffold lands — later release iterations
# add examples/NN-pattern/ one at a time.
file(GLOB CPP_DESIGN_PATTERNS_EXAMPLE_LISTS
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/*/CMakeLists.txt"
)

foreach(_example_list ${CPP_DESIGN_PATTERNS_EXAMPLE_LISTS})
    get_filename_component(_example_dir "${_example_list}" DIRECTORY)
    add_subdirectory("${_example_dir}")
endforeach()

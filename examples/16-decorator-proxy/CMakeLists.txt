# examples/16-decorator-proxy - Decorator and Proxy: a shipping-rate
# decorator stack and a manifest-archive proxy pair.
#
# Decorator (rate_decorators.hpp/.cpp): RateSource is the shared interface.
# TariffTableRateSource is the core; LoggingRateSource, CachingRateSource,
# and SurchargeRateSource each wrap another RateSource and add one
# responsibility, composable in any order.
#
# Proxy (manifest_proxy.hpp/.cpp): ManifestStore is the shared interface.
# ArchivedManifestStore is the real (expensive) subject;
# LazyManifestStoreProxy defers constructing it until first use, and
# AccessControlledManifestProxy gates a lookup behind a caller's clearance,
# both without adding a new capability to what a lookup returns.
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

    project(cpp_design_patterns_16_decorator_proxy LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(decor16_core STATIC
    src/shipping_domain.cpp
    src/rate_decorators.cpp
    src/manifest_proxy.cpp
)
target_include_directories(decor16_core PUBLIC src)
target_compile_features(decor16_core PUBLIC cxx_std_20)

add_executable(decor16_demo src/demo_main.cpp)
target_link_libraries(decor16_demo PRIVATE decor16_core)

add_executable(decor16_tests
    tests/rate_decorators_test.cpp
    tests/manifest_proxy_test.cpp
)
target_link_libraries(decor16_tests PRIVATE decor16_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(decor16_tests)

# examples/04-solid-dependency-inversion - ISP + DIP: order notifications.
#
# A fat "gateway" interface split into three focused role interfaces
# (Interface Segregation), plus two high-level policies that depend on
# those abstractions rather than on a concrete email or SMS gateway
# (Dependency Inversion), wired together by hand in demo_main.cpp's
# composition root, with no DI container anywhere:
#   - MessageSender, TemplateRenderer, DeliveryStatusProvider: role
#     interfaces (gateway_interfaces.hpp)
#   - EmailGateway: implements all three (it has all three
#     capabilities)
#   - SmsGateway: implements only MessageSender
#   - OrderNotificationPolicy: high-level policy, depends on MessageSender
#   - TemplatedOrderNotifier: high-level policy, depends on
#     TemplateRenderer + MessageSender
#   - DeliveryAuditor: high-level policy, depends on
#     DeliveryStatusProvider
#
# Target names are prefixed solid2_ so this example's targets never
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

    project(cpp_design_patterns_04_solid_dependency_inversion LANGUAGES CXX)

    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    enable_testing()
endif()

find_package(GTest REQUIRED)

add_library(solid2_notification_core STATIC
    src/notification_domain.cpp
    src/email_gateway.cpp
    src/sms_gateway.cpp
    src/order_notification_policy.cpp
    src/templated_order_notifier.cpp
    src/delivery_auditor.cpp
)
target_include_directories(solid2_notification_core PUBLIC src)
target_compile_features(solid2_notification_core PUBLIC cxx_std_20)

add_executable(solid2_notification_demo src/demo_main.cpp)
target_link_libraries(solid2_notification_demo PRIVATE solid2_notification_core)

add_executable(solid2_notification_tests
    tests/order_notification_policy_test.cpp
    tests/templated_order_notifier_test.cpp
    tests/delivery_auditor_test.cpp
    tests/gateways_test.cpp
)
target_include_directories(solid2_notification_tests PRIVATE tests)
target_link_libraries(solid2_notification_tests PRIVATE solid2_notification_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(solid2_notification_tests)

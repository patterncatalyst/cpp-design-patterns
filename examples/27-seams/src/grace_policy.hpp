#pragma once

#include <chrono>

namespace seam27 {

// grace_period() is declared here and defined in exactly one translation
// unit, chosen by whichever target's CMakeLists.txt compiles it in:
// grace_policy_production.cpp for seam27_demo, grace_policy_test_double.cpp
// (under tests/) for seam27_tests. late_fee_schedule.cpp, which calls this
// function, is compiled exactly once into seam27_core and links against
// whichever definition its final executable supplies. Neither
// grace_period()'s declaration nor late_fee_schedule.cpp changes between
// the two; only the linker's choice of object file changes. That swap is
// the link seam.
std::chrono::minutes grace_period();

}  // namespace seam27

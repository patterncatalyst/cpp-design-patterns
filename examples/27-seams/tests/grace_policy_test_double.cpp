#include "grace_policy.hpp"

namespace seam27 {

std::chrono::minutes grace_period() {
    // The link seam in action: this definition, not
    // grace_policy_production.cpp's, is the one seam27_tests links
    // against, because its CMakeLists.txt target compiles this file
    // instead of the production one. late_fee_schedule.cpp, compiled once
    // into seam27_core, has no idea which definition it will eventually
    // resolve against at link time.
    return std::chrono::minutes{0};
}

}  // namespace seam27

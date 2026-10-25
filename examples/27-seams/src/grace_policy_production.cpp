#include "grace_policy.hpp"

namespace seam27 {

std::chrono::minutes grace_period() {
    // The enterprise billing policy, carried over from the inherited
    // batch job exactly as found: fifteen minutes of grace before a late
    // shipment starts accruing a fee. Only seam27_demo compiles this file,
    // so this is the definition the shipped binary links against.
    return std::chrono::minutes{15};
}

}  // namespace seam27

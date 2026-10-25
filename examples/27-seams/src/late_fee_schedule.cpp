#include "late_fee_schedule.hpp"

#include "grace_policy.hpp"

namespace seam27 {
namespace {

constexpr int kCentsPerLateMinute = 150;

}  // namespace

int late_fee_cents(int minutes_late) {
    const int grace_minutes = static_cast<int>(grace_period().count());
    const int effective_minutes = minutes_late - grace_minutes;
    if (effective_minutes <= 0) {
        return 0;
    }
    return effective_minutes * kCentsPerLateMinute;
}

}  // namespace seam27

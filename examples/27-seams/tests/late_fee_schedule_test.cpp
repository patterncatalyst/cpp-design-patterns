#include "late_fee_schedule.hpp"

#include <gtest/gtest.h>

namespace seam27 {
namespace {

// seam27_tests compiles grace_policy_test_double.cpp instead of
// grace_policy_production.cpp, so grace_period() returns zero minutes in
// this binary, not production's fifteen. late_fee_cents() itself, and the
// header it is declared in, are completely unaware of the substitution;
// the swap happened entirely in which .cpp file this executable's
// CMakeLists.txt target chose to compile.

TEST(LateFeeScheduleTest, ZeroMinutesLateNeverAccruesAFee) {
    EXPECT_EQ(late_fee_cents(0), 0);
}

// This is the test that actually proves the link seam took effect. With
// production's fifteen-minute grace period, five minutes late would still
// be inside the grace window and would accrue nothing. The nonzero answer
// here is only correct because this binary links the test double's
// zero-minute grace period instead.
TEST(LateFeeScheduleTest, FiveMinutesLateAccruesAFeeBecauseThisBinaryUsesTheZeroMinuteTestDouble) {
    EXPECT_EQ(late_fee_cents(5), 750);
}

TEST(LateFeeScheduleTest, FeeScalesLinearlyWithMinutesLate) {
    EXPECT_EQ(late_fee_cents(10), 1500);
    EXPECT_EQ(late_fee_cents(20), 3000);
}

}  // namespace
}  // namespace seam27

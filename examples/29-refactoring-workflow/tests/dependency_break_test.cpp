// Stage 2: the dependency-break test. SeamedEtaEstimator applies two
// Feathers techniques to LegacyEtaEstimator: Extract Interface pulled
// Clock out of the wall-clock read, and Parameterize Constructor added an
// overload that accepts one. Everything this test checks was previously
// impossible to check: an exact delivery_date, an exact holiday-adjusted
// transit_days, and an exact holiday-adjusted surcharge, all now pinned
// because "today" is a FixedClock value instead of whatever day the
// suite happens to run on.
//
// This file also re-asserts every characterization value from stage one
// against the same shipments, now routed through SeamedEtaEstimator with
// a non-holiday FixedClock. The carrier dispatch and surcharge math were
// never touched by this stage; the assertions below are the proof.

#include <chrono>

#include <gtest/gtest.h>

#include "seamed_eta_estimator.hpp"

namespace refactor29 {
namespace {

const std::chrono::year_month_day kJuly4 = std::chrono::year{2026} / std::chrono::July / 4;
const std::chrono::year_month_day kDecember15 =
    std::chrono::year{2026} / std::chrono::December / 15;

TEST(DependencyBreak, NonHolidayDateProducesNoSeasonalAdjustment) {
    const FixedClock clock(kJuly4);
    const SeamedEtaEstimator estimator(clock);

    const auto result = estimator.estimate("ground", "A", "A", 10.0, false, 200.0);

    EXPECT_EQ(result.transit_days, 2);
    EXPECT_DOUBLE_EQ(result.surcharge, 0.0);
    EXPECT_EQ(result.delivery_date, std::chrono::year{2026} / std::chrono::July / 6);
}

TEST(DependencyBreak, HolidayDateAddsTwoDaysAndEightDollars) {
    const FixedClock clock(kDecember15);
    const SeamedEtaEstimator estimator(clock);

    const auto result = estimator.estimate("ground", "A", "A", 10.0, false, 200.0);

    EXPECT_EQ(result.transit_days, 4);   // 2 base + 2 holiday
    EXPECT_DOUBLE_EQ(result.surcharge, 8.0);  // 0 base + 8 holiday
    EXPECT_EQ(result.delivery_date, std::chrono::year{2026} / std::chrono::December / 19);
}

TEST(DependencyBreak, PreservesCarrierDispatchFromStageOne) {
    const FixedClock clock(kJuly4);  // non-holiday, so no seasonal adjustment interferes
    const SeamedEtaEstimator estimator(clock);

    EXPECT_EQ(estimator.estimate("ground", "A", "B", 10.0, false, 200.0).transit_days, 5);
    EXPECT_EQ(estimator.estimate("air", "A", "B", 10.0, false, 200.0).transit_days, 2);
    EXPECT_EQ(estimator.estimate("air", "A", "A", 10.0, false, 200.0).transit_days, 1);
    EXPECT_EQ(estimator.estimate("express", "A", "B", 10.0, false, 200.0).transit_days, 1);
}

TEST(DependencyBreak, PreservesSurchargeRulesFromStageOne) {
    const FixedClock clock(kJuly4);
    const SeamedEtaEstimator estimator(clock);

    const auto heavy_residential_high_value =
        estimator.estimate("ground", "A", "A", 55.0, true, 1500.0);
    EXPECT_EQ(heavy_residential_high_value.transit_days, 3);  // signature day included
    EXPECT_DOUBLE_EQ(heavy_residential_high_value.surcharge, 32.50);
}

TEST(DependencyBreak, DefaultConstructorStillBuildsAProductionClock) {
    // Parameterize Constructor kept the no-argument constructor working:
    // existing production call sites that never pass a Clock still
    // compile and still get a real SystemClock under the hood.
    const SeamedEtaEstimator production_estimator;
    const auto result = production_estimator.estimate("express", "A", "A", 1.0, false, 50.0);
    EXPECT_EQ(result.transit_days, 1);
}

}  // namespace
}  // namespace refactor29

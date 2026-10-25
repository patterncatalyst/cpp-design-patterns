// Stage 1: characterization tests. These pin exactly what the inherited
// code's current behavior is, before anything about it changes.
//
// transit_calculations.hpp's two free functions are already testable:
// they take plain values, return plain values, and read no hidden
// collaborator. Pinning them first establishes the baseline every later
// stage has to reproduce.
//
// LegacyEtaEstimator::estimate() is a different story. It reads
// std::chrono::system_clock::now() on its own, with no way for a test to
// supply a different "today." A characterization test can still say
// something true about it (its transit-day and surcharge math is
// consistent with the deterministic rule, give or take the one seasonal
// adjustment that depends on the calendar), but it cannot pin an exact
// transit_days or delivery_date the way it pinned base_transit_days()
// above, because that answer silently changes every November. That gap
// is exactly what the next stage's dependency break exists to close.

#include <chrono>

#include <gtest/gtest.h>

#include "legacy_eta_estimator.hpp"
#include "transit_calculations.hpp"

namespace refactor29 {
namespace {

TEST(TransitCalculationsCharacterization, GroundSameZoneIsTwoDays) {
    EXPECT_EQ(base_transit_days("ground", "A", "A", 200.0), 2);
}

TEST(TransitCalculationsCharacterization, GroundDifferentZoneIsFiveDays) {
    EXPECT_EQ(base_transit_days("ground", "A", "B", 200.0), 5);
}

TEST(TransitCalculationsCharacterization, AirDifferentZoneIsTwoDays) {
    EXPECT_EQ(base_transit_days("air", "A", "B", 200.0), 2);
}

TEST(TransitCalculationsCharacterization, AirSameZoneIsOneDay) {
    EXPECT_EQ(base_transit_days("air", "A", "A", 200.0), 1);
}

TEST(TransitCalculationsCharacterization, ExpressIsAlwaysOneDayRegardlessOfZone) {
    EXPECT_EQ(base_transit_days("express", "A", "B", 200.0), 1);
    EXPECT_EQ(base_transit_days("express", "A", "A", 200.0), 1);
}

TEST(TransitCalculationsCharacterization, UnrecognizedCarrierFallsBackToFiveDays) {
    EXPECT_EQ(base_transit_days("freight", "A", "B", 200.0), 5);
}

TEST(TransitCalculationsCharacterization, HighValueShipmentAddsASignatureDay) {
    EXPECT_EQ(base_transit_days("ground", "A", "A", 1500.0), 3);
}

TEST(TransitCalculationsCharacterization, StandardShipmentHasNoSurcharge) {
    EXPECT_DOUBLE_EQ(base_surcharge(10.0, false, 200.0), 0.0);
}

TEST(TransitCalculationsCharacterization, HeavyResidentialHighValueStacksSurcharges) {
    EXPECT_DOUBLE_EQ(base_surcharge(55.0, true, 1500.0), 32.50);
}

// LegacyEtaEstimator::estimate() cannot be pinned to an exact number the
// way the two free functions above were: it reads "today" on its own, and
// November/December push two extra transit days and an $8 surcharge into
// an answer this test has no way to predict in advance. What this test
// can still pin is the invariant that holds regardless of the calendar:
// the result is either the deterministic baseline or that baseline plus
// the holiday adjustment, never anything else, and the delivery date is
// always base-clock-today plus however many transit days were reported.
TEST(LegacyEtaEstimatorCharacterization, ResultIsBaselineOrBaselinePlusHolidayAdjustment) {
    const LegacyEtaEstimator estimator;
    const auto result = estimator.estimate("ground", "A", "A", 10.0, false, 200.0);

    const int baseline_days = base_transit_days("ground", "A", "A", 200.0);
    const double baseline_surcharge = base_surcharge(10.0, false, 200.0);

    EXPECT_TRUE(result.transit_days == baseline_days || result.transit_days == baseline_days + 2)
        << "transit_days was " << result.transit_days;
    EXPECT_TRUE(result.surcharge == baseline_surcharge || result.surcharge == baseline_surcharge + 8.0)
        << "surcharge was " << result.surcharge;

    const auto now = std::chrono::system_clock::now();
    const auto today =
        std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(now)};
    const auto expected_delivery = std::chrono::year_month_day{
        std::chrono::sys_days{today} + std::chrono::days{result.transit_days}};
    EXPECT_EQ(result.delivery_date, expected_delivery);
}

}  // namespace
}  // namespace refactor29

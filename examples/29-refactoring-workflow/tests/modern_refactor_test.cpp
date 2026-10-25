// Stage 3: Fowler's named refactorings, applied to SeamedEtaEstimator now
// that a characterization suite and a clock seam both exist. Extract
// Function split the holiday rule and the surcharge rule out of
// estimate()'s body; Replace Conditional with Polymorphism retired the
// carrier if/else chain in favor of CarrierProfile; Introduce Parameter
// Object retired the six-argument call in favor of ShipmentProfile. The
// clock seam itself is untouched.
//
// Every test below runs the same inputs through SeamedEtaEstimator (the
// stage-two result) and ModernEtaEstimator (the stage-three result) and
// checks they agree exactly. That agreement is what makes each named
// refactoring trustworthy: restructuring the code changed nothing a
// caller can observe.

#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include "modern_eta_estimator.hpp"
#include "seamed_eta_estimator.hpp"
#include "shipment_profile.hpp"

namespace refactor29 {
namespace {

const std::chrono::year_month_day kJuly4 = std::chrono::year{2026} / std::chrono::July / 4;
const std::chrono::year_month_day kDecember15 =
    std::chrono::year{2026} / std::chrono::December / 15;

TEST(ModernRefactor, MatchesSeamedEstimatorOnNonHolidayGroundShipment) {
    const FixedClock clock(kJuly4);
    const SeamedEtaEstimator seamed(clock);
    const ModernEtaEstimator modern(clock);

    const auto seamed_result = seamed.estimate("ground", "A", "A", 10.0, false, 200.0);
    const auto modern_result = modern.estimate(ShipmentProfile{"ground", "A", "A", 10.0, false, 200.0});

    EXPECT_EQ(modern_result.transit_days, seamed_result.transit_days);
    EXPECT_DOUBLE_EQ(modern_result.surcharge, seamed_result.surcharge);
    EXPECT_EQ(modern_result.delivery_date, seamed_result.delivery_date);
}

TEST(ModernRefactor, MatchesSeamedEstimatorOnHolidayShipment) {
    const FixedClock clock(kDecember15);
    const SeamedEtaEstimator seamed(clock);
    const ModernEtaEstimator modern(clock);

    const auto seamed_result = seamed.estimate("ground", "A", "A", 10.0, false, 200.0);
    const auto modern_result = modern.estimate(ShipmentProfile{"ground", "A", "A", 10.0, false, 200.0});

    EXPECT_EQ(modern_result.transit_days, 4);
    EXPECT_EQ(modern_result.transit_days, seamed_result.transit_days);
    EXPECT_DOUBLE_EQ(modern_result.surcharge, 8.0);
    EXPECT_DOUBLE_EQ(modern_result.surcharge, seamed_result.surcharge);
    EXPECT_EQ(modern_result.delivery_date, seamed_result.delivery_date);
}

TEST(ModernRefactor, MatchesSeamedEstimatorAcrossEveryExistingCarrier) {
    const FixedClock clock(kJuly4);
    const SeamedEtaEstimator seamed(clock);
    const ModernEtaEstimator modern(clock);

    const struct {
        std::string carrier;
        std::string origin;
        std::string destination;
    } routes[] = {
        {"ground", "A", "A"}, {"ground", "A", "B"}, {"air", "A", "A"},
        {"air", "A", "B"},    {"express", "A", "B"},
    };

    for (const auto& route : routes) {
        const auto seamed_result =
            seamed.estimate(route.carrier, route.origin, route.destination, 10.0, false, 200.0);
        const auto modern_result = modern.estimate(
            ShipmentProfile{route.carrier, route.origin, route.destination, 10.0, false, 200.0});

        EXPECT_EQ(modern_result.transit_days, seamed_result.transit_days) << route.carrier;
        EXPECT_DOUBLE_EQ(modern_result.surcharge, seamed_result.surcharge) << route.carrier;
    }
}

TEST(ModernRefactor, MatchesSeamedEstimatorOnStackedSurcharges) {
    const FixedClock clock(kJuly4);
    const SeamedEtaEstimator seamed(clock);
    const ModernEtaEstimator modern(clock);

    const auto seamed_result = seamed.estimate("ground", "A", "A", 55.0, true, 1500.0);
    const auto modern_result =
        modern.estimate(ShipmentProfile{"ground", "A", "A", 55.0, true, 1500.0});

    EXPECT_EQ(modern_result.transit_days, 3);
    EXPECT_DOUBLE_EQ(modern_result.surcharge, 32.50);
    EXPECT_EQ(modern_result.transit_days, seamed_result.transit_days);
    EXPECT_DOUBLE_EQ(modern_result.surcharge, seamed_result.surcharge);
}

// Replace Conditional with Polymorphism's payoff: a fourth carrier is one
// new CarrierProfile subclass and one new line in carrier_profile_for(),
// not an edit to GroundCarrierProfile, AirCarrierProfile, or
// ExpressCarrierProfile. SeamedEtaEstimator, which still dispatches
// through the original if/else chain, has no equivalent for "freight" at
// all; the if/else chain's unrecognized-carrier fallback would answer 5,
// not the dedicated 6-day rule freight actually needs.
TEST(ModernRefactor, NewCarrierExtendsWithoutEditingExistingProfiles) {
    const FixedClock clock(kJuly4);
    const ModernEtaEstimator modern(clock);

    const auto result = modern.estimate(ShipmentProfile{"freight", "A", "B", 10.0, false, 200.0});

    EXPECT_EQ(result.transit_days, 6);
    EXPECT_DOUBLE_EQ(result.surcharge, 0.0);
    EXPECT_EQ(result.delivery_date, std::chrono::year{2026} / std::chrono::July / 10);
}

TEST(ModernRefactor, DefaultConstructorStillBuildsAProductionClock) {
    const ModernEtaEstimator production_estimator;
    const auto result =
        production_estimator.estimate(ShipmentProfile{"express", "A", "A", 1.0, false, 50.0});
    EXPECT_EQ(result.transit_days, 1);
}

}  // namespace
}  // namespace refactor29

#include "policy_strategy.hpp"

#include <gtest/gtest.h>

namespace shipping {
namespace {

TEST(StaticRateCalculatorTest, FlatRatePolicySelectedAtCompileTime) {
    StaticRateCalculator<FlatRatePolicy> calculator{FlatRatePolicy{999}};
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    EXPECT_EQ(calculator.estimate(shipment), 999);
}

TEST(StaticRateCalculatorTest, WeightBasedPolicyMatchesTheOtherExpressions) {
    StaticRateCalculator<WeightBasedPolicy> calculator{WeightBasedPolicy{500, 150}};
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    EXPECT_EQ(calculator.estimate(shipment), 1025);
}

TEST(StaticRateCalculatorTest, TieredZonePolicyMatchesTheOtherExpressions) {
    StaticRateCalculator<TieredZonePolicy> calculator{TieredZonePolicy{}};
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    EXPECT_EQ(calculator.estimate(shipment), 1230);
}

TEST(RatePolicyConceptTest, RejectsATypeWithoutAQuoteMethod) {
    struct NotAPolicy {};

    EXPECT_FALSE(RatePolicy<NotAPolicy>);
    EXPECT_TRUE(RatePolicy<FlatRatePolicy>);
}

}  // namespace
}  // namespace shipping

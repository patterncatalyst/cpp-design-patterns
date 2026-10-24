#include "runtime_strategy.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace shipping {
namespace {

TEST(FlatRateCarrierTest, ChargesTheSameRegardlessOfShipment) {
    FlatRateCarrier carrier(999);
    const Shipment light{0.5, DestinationZone::Local, 2000};
    const Shipment heavy{40.0, DestinationZone::International, 500000};

    EXPECT_EQ(carrier.quote(light), 999);
    EXPECT_EQ(carrier.quote(heavy), 999);
}

TEST(WeightBasedCarrierTest, ChargesBasePlusPerKilogramRoundedUp) {
    WeightBasedCarrier carrier(/*base_cents=*/500, /*per_kg_cents=*/150);
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    EXPECT_EQ(carrier.quote(shipment), 1025);
}

TEST(WeightBasedCarrierTest, RoundsFractionalCentsUpNeverDown) {
    WeightBasedCarrier carrier(/*base_cents=*/0, /*per_kg_cents=*/100);
    const Shipment shipment{0.333, DestinationZone::Local, 0};

    // 0.333 kg * 100 cents/kg = 33.3 cents, must round up to 34, never
    // truncate to 33, since a carrier never rounds a quote in its own favor
    // against the customer.
    EXPECT_EQ(carrier.quote(shipment), 34);
}

TEST(TieredZoneCarrierTest, ChargesZoneBaseWhenUnderThreshold) {
    TieredZoneCarrier carrier;
    const Shipment shipment{1.0, DestinationZone::National, 5000};

    EXPECT_EQ(carrier.quote(shipment), 1400);
}

TEST(TieredZoneCarrierTest, AddsPerZoneOverageBeyondTwoKilograms) {
    TieredZoneCarrier carrier;
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    EXPECT_EQ(carrier.quote(shipment), 1230);
}

TEST(RateEstimatorTest, DelegatesToTheInjectedStrategy) {
    RateEstimator estimator(std::make_unique<FlatRateCarrier>(999));
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    EXPECT_EQ(estimator.estimate(shipment), 999);
}

TEST(RateEstimatorTest, SwapsStrategyAtRuntime) {
    RateEstimator estimator(std::make_unique<FlatRateCarrier>(999));
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};
    ASSERT_EQ(estimator.estimate(shipment), 999);

    estimator.use_strategy(std::make_unique<TieredZoneCarrier>());

    EXPECT_EQ(estimator.estimate(shipment), 1230);
}

#if __cplusplus >= 202302L
TEST(RateEstimatorTest, TryEstimateRejectsNegativeWeightUnderCpp23) {
    RateEstimator estimator(std::make_unique<FlatRateCarrier>(999));
    const Shipment malformed{-1.0, DestinationZone::Local, 0};

    const auto result = estimator.try_estimate(malformed);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "shipment weight_kg cannot be negative");
}

TEST(RateEstimatorTest, TryEstimateReturnsTheQuoteForAValidShipment) {
    RateEstimator estimator(std::make_unique<FlatRateCarrier>(999));
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    const auto result = estimator.try_estimate(shipment);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 999);
}
#endif

}  // namespace
}  // namespace shipping

#include <gtest/gtest.h>

#include <stdexcept>

#include "flat_rate_strategy.hpp"
#include "shipment.hpp"
#include "weight_tiered_rate_strategy.hpp"

namespace finalproject31 {
namespace {

Shipment make_shipment(double weight_kg) { return Shipment("SHIP-1", "carrier", "LOCAL", weight_kg, 0); }

TEST(FlatRateStrategyTest, QuotesTheSameFeeRegardlessOfWeight) {
    FlatRateStrategy strategy(899);

    EXPECT_EQ(strategy.quote(make_shipment(1.0)), 899);
    EXPECT_EQ(strategy.quote(make_shipment(40.0)), 899);
}

TEST(WeightTieredRateStrategyTest, QuotesTheMatchingTierForALightShipment) {
    WeightTieredRateStrategy strategy({
        WeightTier{.max_weight_kg = 5.0, .base_cents = 1200, .cents_per_kg = 180.0},
        WeightTier{.max_weight_kg = 20.0, .base_cents = 1600, .cents_per_kg = 140.0},
    });

    // 3kg falls in the first tier: 1200 + round(180 * 3) = 1200 + 540 = 1740.
    EXPECT_EQ(strategy.quote(make_shipment(3.0)), 1740);
}

TEST(WeightTieredRateStrategyTest, QuotesTheMatchingTierForAHeavierShipment) {
    WeightTieredRateStrategy strategy({
        WeightTier{.max_weight_kg = 5.0, .base_cents = 1200, .cents_per_kg = 180.0},
        WeightTier{.max_weight_kg = 20.0, .base_cents = 1600, .cents_per_kg = 140.0},
    });

    // 15kg falls in the second tier: 1600 + round(140 * 15) = 1600 + 2100 = 3700.
    EXPECT_EQ(strategy.quote(make_shipment(15.0)), 3700);
}

TEST(WeightTieredRateStrategyTest, ShipmentsHeavierThanEveryTierUseTheLastTier) {
    WeightTieredRateStrategy strategy({
        WeightTier{.max_weight_kg = 5.0, .base_cents = 1200, .cents_per_kg = 180.0},
        WeightTier{.max_weight_kg = 20.0, .base_cents = 1600, .cents_per_kg = 140.0},
    });

    // 100kg exceeds every ceiling, so it still bills under the last tier:
    // 1600 + round(140 * 100) = 1600 + 14000 = 15600.
    EXPECT_EQ(strategy.quote(make_shipment(100.0)), 15600);
}

TEST(WeightTieredRateStrategyTest, RejectsAnEmptyTierTable) {
    EXPECT_THROW(WeightTieredRateStrategy({}), std::invalid_argument);
}

}  // namespace
}  // namespace finalproject31

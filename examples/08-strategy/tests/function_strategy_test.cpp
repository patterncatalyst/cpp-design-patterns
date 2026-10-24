#include "function_strategy.hpp"

#include <gtest/gtest.h>

namespace shipping {
namespace {

TEST(FunctionStrategyTest, FlatRateIgnoresShipmentDetails) {
    const RateFn rate = flat_rate(999);
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    EXPECT_EQ(rate(shipment), 999);
}

TEST(FunctionStrategyTest, WeightBasedMatchesThePolymorphicCarrier) {
    const RateFn rate = weight_based(500, 150);
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    EXPECT_EQ(rate(shipment), 1025);
}

TEST(FunctionStrategyTest, TieredZoneMatchesThePolymorphicCarrier) {
    const RateFn rate = tiered_zone();
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    EXPECT_EQ(rate(shipment), 1230);
}

TEST(FunctionStrategyTest, InsuranceSurchargeComposesOnTopOfTheBaseRate) {
    const RateFn rate = with_insurance_surcharge(tiered_zone(), /*cents_per_declared_dollar=*/1.5);
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};

    // Base tiered quote is 1230 cents; declared value is $120.00, so the
    // surcharge is ceil(120 * 1.5) = 180 cents on top.
    EXPECT_EQ(rate(shipment), 1410);
}

TEST(QuoteBookTest, SwapsTheUnderlyingCallableAtRuntime) {
    QuoteBook book(flat_rate(999));
    const Shipment shipment{3.5, DestinationZone::Regional, 12000};
    ASSERT_EQ(book.quote(shipment), 999);

    book.use(weight_based(500, 150));

    EXPECT_EQ(book.quote(shipment), 1025);
}

}  // namespace
}  // namespace shipping

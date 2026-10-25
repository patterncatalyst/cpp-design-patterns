#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "shipment_operations_manager.hpp"
#include "shipment_pricing_service.hpp"
#include "shipping_rate_registry.hpp"

namespace antipattern26 {
namespace {

// Proof #1 this chapter claims: the over-patterned design and the
// simplified design compute the identical shipment total for the same
// inputs. Every difference this chapter demonstrates elsewhere is a
// difference in design, not in the arithmetic.
TEST(AntiPatternEquivalenceTest, OverPatternedAndSimplifiedComputeIdenticalTotals) {
    struct Case {
        double base_cost;
        double surcharge_multiplier;
    };
    const std::vector<Case> cases = {
        {100.0, 1.0},
        {250.0, 1.15},
        {40.0, 1.35},
        {999.99, 1.0},
    };

    ShipmentOperationsManager manager;

    for (const auto& c : cases) {
        ShippingRateRegistry::instance().set_surcharge_multiplier(c.surcharge_multiplier);
        const PricedShipmentReceipt receipt = manager.price_shipment("ORD-EQ", c.base_cost);

        const ShipmentPricingService service(RateSettings{c.surcharge_multiplier, 0.0});
        const PricingResult result = service.price(c.base_cost);

        EXPECT_DOUBLE_EQ(receipt.total_cost, result.total_cost);
    }
}

TEST(AntiPatternEquivalenceTest, BothVersionsRejectANonPositiveBaseCostTheSameWay) {
    ShipmentOperationsManager manager;
    const ShipmentPricingService service(RateSettings{1.0, 0.0});

    EXPECT_THROW(manager.price_shipment("ORD-BAD", 0.0), std::invalid_argument);
    EXPECT_THROW(service.price(0.0), std::invalid_argument);
    EXPECT_THROW(manager.price_shipment("ORD-BAD", -5.0), std::invalid_argument);
    EXPECT_THROW(service.price(-5.0), std::invalid_argument);
}

}  // namespace
}  // namespace antipattern26

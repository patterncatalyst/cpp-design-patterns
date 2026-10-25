#include <gtest/gtest.h>

#include <stdexcept>

#include "shipment_operations_manager.hpp"
#include "shipping_rate_registry.hpp"

namespace antipattern26 {
namespace {

// The leaky abstraction, made concrete: a caller holds receipt_a, priced
// with no surcharge, and later asks the manager whether a surcharge was
// applied. The manager answers for whichever shipment was priced most
// recently, ORD-3002, not for the receipt the caller is actually holding,
// ORD-3001. The method's name promises an answer about "the last
// calculation"; a caller reading receipt_a's variable name expects an
// answer about ORD-3001. Those are different shipments by the time this
// assertion runs.
TEST(ShipmentOperationsManagerTest, SurchargeFlagLeaksTheManagersCallOrderInsteadOfTheReceipts) {
    ShipmentOperationsManager manager;

    ShippingRateRegistry::instance().set_surcharge_multiplier(1.0);
    const PricedShipmentReceipt receipt_a = manager.price_shipment("ORD-3001", 100.0);
    EXPECT_FALSE(manager.was_last_calculation_surcharge_applied());

    ShippingRateRegistry::instance().set_surcharge_multiplier(1.25);
    const PricedShipmentReceipt receipt_b = manager.price_shipment("ORD-3002", 100.0);

    // A caller who still holds receipt_a and checks this flag expecting to
    // learn about ORD-3001 instead learns about ORD-3002.
    EXPECT_TRUE(manager.was_last_calculation_surcharge_applied());
    EXPECT_EQ(receipt_a.order_id, "ORD-3001");
    EXPECT_EQ(receipt_b.order_id, "ORD-3002");
}

TEST(ShipmentOperationsManagerTest, PriceShipmentRejectsANonPositiveBaseCost) {
    ShipmentOperationsManager manager;
    EXPECT_THROW(manager.price_shipment("ORD-BAD", 0.0), std::invalid_argument);
}

// The god object's logging responsibility, exercised directly: this test
// has to know about operation_log() purely to confirm a pricing call
// happened, a responsibility that has nothing to do with "was this
// shipment priced correctly."
TEST(ShipmentOperationsManagerTest, PricingAShipmentAlsoWritesAnAuditLogEntry) {
    ShipmentOperationsManager manager;
    ShippingRateRegistry::instance().set_surcharge_multiplier(1.0);

    EXPECT_TRUE(manager.operation_log().empty());
    manager.price_shipment("ORD-3003", 50.0);
    EXPECT_EQ(manager.operation_log().size(), 1U);
}

}  // namespace
}  // namespace antipattern26

#include "shipment_dispatcher.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>

#include "lifecycle_observer.hpp"
#include "shipment.hpp"
#include "shipment_types.hpp"

// These tests are this example's actual point: they prove the five
// patterns work TOGETHER through ShipmentDispatcher, not each one in
// isolation. Every other test file in this example already covers a
// single pattern's own contract; this file never repeats that, it only
// checks what happens at the seams between them.
namespace finalproject31 {
namespace {

using ::testing::InSequence;

class MockLifecycleObserver : public LifecycleObserver {
public:
    MOCK_METHOD(void, on_transitioned, (const Shipment& shipment, ShipmentStatus from, ShipmentStatus to),
                (override));
};

TEST(ShipmentDispatcherIntegrationTest, FactoryBuiltStrategyPricesTheOpeningQuote) {
    ShipmentDispatcher dispatcher;

    Shipment& shipment = dispatcher.open_shipment("SHIP-4821", CarrierRequest{"SKYHAUL", "NATIONAL"}, 8.0);

    // Factory: CarrierFactory recognized "SKYHAUL" and built Skyhaul's
    // profile, not some other carrier's.
    EXPECT_EQ(dispatcher.carrier_profile().carrier_name, "Skyhaul Express");
    // Strategy: the opening quote is whatever Skyhaul's WeightTieredRateStrategy
    // computes for 8kg (second tier: 1600 + round(140*8) = 2720), not a
    // number the dispatcher invented on its own.
    EXPECT_EQ(shipment.quoted_rate_cents(), 2720);
}

TEST(ShipmentDispatcherIntegrationTest, OpeningAnUnrecognizedCarrierPropagatesTheFactoryRejection) {
    ShipmentDispatcher dispatcher;

    EXPECT_THROW(dispatcher.open_shipment("SHIP-1", CarrierRequest{"PIGEON-POST", "LOCAL"}, 1.0),
                 std::invalid_argument);
}

TEST(ShipmentDispatcherIntegrationTest, LifecycleCommandsFireObserverEventsInOrderAsStatusAdvances) {
    ShipmentDispatcher dispatcher;
    dispatcher.open_shipment("SHIP-4821", CarrierRequest{"SKYHAUL", "NATIONAL"}, 8.0);

    MockLifecycleObserver observer;
    dispatcher.subscribe(observer);

    {
        InSequence sequence;
        EXPECT_CALL(observer, on_transitioned(testing::Ref(dispatcher.shipment()), ShipmentStatus::Created,
                                               ShipmentStatus::PickedUp))
            .Times(1);
        EXPECT_CALL(observer, on_transitioned(testing::Ref(dispatcher.shipment()), ShipmentStatus::PickedUp,
                                               ShipmentStatus::InTransit))
            .Times(1);
    }

    // Command: each apply() runs an AdvanceLifecycleCommand through the
    // dispatcher's CommandHistory. State: the command's execute() calls
    // into Shipment::advance(), the one legal way the status changes.
    // Observer: that same successful transition is what fires the
    // notification the mock above is watching for, in the order it
    // happened.
    dispatcher.apply(dispatcher.make_advance_command(ShipmentEvent::PickUp));
    dispatcher.apply(dispatcher.make_advance_command(ShipmentEvent::Dispatch));

    EXPECT_EQ(dispatcher.shipment().status(), ShipmentStatus::InTransit);
}

TEST(ShipmentDispatcherIntegrationTest, WeightCorrectionRecomputesViaStrategyAndUndoRestoresBoth) {
    ShipmentDispatcher dispatcher;
    dispatcher.open_shipment("SHIP-4821", CarrierRequest{"SKYHAUL", "NATIONAL"}, 8.0);
    const Shipment& shipment = dispatcher.shipment();
    const double original_weight = shipment.weight_kg();
    const Cents original_quote = shipment.quoted_rate_cents();

    // Command invoking Strategy: AdjustWeightCommand closes over the same
    // RateStrategy CarrierFactory built for this carrier and asks it for a
    // fresh quote at the corrected weight (second tier: 1600 +
    // round(140*15) = 3700).
    dispatcher.apply(dispatcher.make_adjust_weight_command(15.0));

    EXPECT_DOUBLE_EQ(shipment.weight_kg(), 15.0);
    EXPECT_EQ(shipment.quoted_rate_cents(), 3700);

    // Command undo: CommandHistory reverses the most recently executed
    // command, restoring both the weight and the quote to their exact
    // pre-correction values without asking the strategy for anything.
    ASSERT_TRUE(dispatcher.undo_last());

    EXPECT_DOUBLE_EQ(shipment.weight_kg(), original_weight);
    EXPECT_EQ(shipment.quoted_rate_cents(), original_quote);
}

TEST(ShipmentDispatcherIntegrationTest, UndoingALifecycleCommandRestoresStatusAndFiresACorrectionEvent) {
    ShipmentDispatcher dispatcher;
    dispatcher.open_shipment("SHIP-4821", CarrierRequest{"SKYHAUL", "NATIONAL"}, 8.0);

    MockLifecycleObserver observer;
    dispatcher.subscribe(observer);

    {
        InSequence sequence;
        EXPECT_CALL(observer, on_transitioned(testing::Ref(dispatcher.shipment()), ShipmentStatus::Created,
                                               ShipmentStatus::PickedUp))
            .Times(1);
        EXPECT_CALL(observer, on_transitioned(testing::Ref(dispatcher.shipment()), ShipmentStatus::PickedUp,
                                               ShipmentStatus::Created))
            .Times(1);
    }

    dispatcher.apply(dispatcher.make_advance_command(ShipmentEvent::PickUp));
    ASSERT_TRUE(dispatcher.undo_last());

    EXPECT_EQ(dispatcher.shipment().status(), ShipmentStatus::Created);
}

TEST(ShipmentDispatcherIntegrationTest, FullRunComposesAllFivePatternsEndToEnd) {
    ShipmentDispatcher dispatcher;
    dispatcher.open_shipment("SHIP-4821", CarrierRequest{"SKYHAUL", "NATIONAL"}, 8.0);
    const Shipment& shipment = dispatcher.shipment();

    MockLifecycleObserver observer;
    dispatcher.subscribe(observer);
    EXPECT_CALL(observer, on_transitioned(testing::Ref(shipment), testing::_, testing::_)).Times(2);

    // Factory + Strategy: the opening quote already reflects Skyhaul's own
    // weight-tiered rule.
    EXPECT_EQ(shipment.quoted_rate_cents(), 2720);

    // State + Command + Observer: two legal transitions, two notifications.
    dispatcher.apply(dispatcher.make_advance_command(ShipmentEvent::PickUp));
    dispatcher.apply(dispatcher.make_advance_command(ShipmentEvent::Dispatch));
    EXPECT_EQ(shipment.status(), ShipmentStatus::InTransit);

    // Command + Strategy: a correction re-quotes through the same strategy.
    dispatcher.apply(dispatcher.make_adjust_weight_command(15.0));
    EXPECT_EQ(shipment.quoted_rate_cents(), 3700);

    // Command undo: the correction reverses cleanly, leaving the lifecycle
    // transitions above untouched.
    ASSERT_TRUE(dispatcher.undo_last());
    EXPECT_DOUBLE_EQ(shipment.weight_kg(), 8.0);
    EXPECT_EQ(shipment.quoted_rate_cents(), 2720);
    EXPECT_EQ(shipment.status(), ShipmentStatus::InTransit);
}

}  // namespace
}  // namespace finalproject31

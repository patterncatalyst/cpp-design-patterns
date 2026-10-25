#include "advance_lifecycle_command.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>

#include "lifecycle_observer.hpp"
#include "shipment.hpp"

namespace finalproject31 {
namespace {

using ::testing::InSequence;

class MockLifecycleObserver : public LifecycleObserver {
public:
    MOCK_METHOD(void, on_transitioned, (const Shipment& shipment, ShipmentStatus from, ShipmentStatus to),
                (override));
};

TEST(AdvanceLifecycleCommandTest, ExecuteAdvancesStatusAndNotifiesObserver) {
    Shipment shipment("SHIP-1", "carrier", "LOCAL", 3.0, 0);
    MockLifecycleObserver observer;
    shipment.subscribe(observer);
    AdvanceLifecycleCommand command(shipment, ShipmentEvent::PickUp);

    EXPECT_CALL(observer, on_transitioned(testing::Ref(shipment), ShipmentStatus::Created, ShipmentStatus::PickedUp))
        .Times(1);

    command.execute();

    EXPECT_EQ(shipment.status(), ShipmentStatus::PickedUp);
}

TEST(AdvanceLifecycleCommandTest, UndoRestoresThePriorStatusAndNotifiesTheCorrection) {
    Shipment shipment("SHIP-1", "carrier", "LOCAL", 3.0, 0);
    MockLifecycleObserver observer;
    shipment.subscribe(observer);
    AdvanceLifecycleCommand command(shipment, ShipmentEvent::PickUp);

    {
        InSequence sequence;
        EXPECT_CALL(observer,
                    on_transitioned(testing::Ref(shipment), ShipmentStatus::Created, ShipmentStatus::PickedUp))
            .Times(1);
        EXPECT_CALL(observer,
                    on_transitioned(testing::Ref(shipment), ShipmentStatus::PickedUp, ShipmentStatus::Created))
            .Times(1);
    }

    command.execute();
    command.undo();

    EXPECT_EQ(shipment.status(), ShipmentStatus::Created);
}

TEST(AdvanceLifecycleCommandTest, UndoBeforeExecuteThrows) {
    Shipment shipment("SHIP-1", "carrier", "LOCAL", 3.0, 0);
    AdvanceLifecycleCommand command(shipment, ShipmentEvent::PickUp);

    EXPECT_THROW(command.undo(), std::logic_error);
}

TEST(AdvanceLifecycleCommandTest, ExecutePropagatesAnIllegalTransition) {
    Shipment shipment("SHIP-1", "carrier", "LOCAL", 3.0, 0);
    AdvanceLifecycleCommand command(shipment, ShipmentEvent::Deliver);

    EXPECT_THROW(command.execute(), IllegalTransitionError);
}

}  // namespace
}  // namespace finalproject31

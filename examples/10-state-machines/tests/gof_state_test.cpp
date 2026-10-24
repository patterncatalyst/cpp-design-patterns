#include "gof_state.hpp"

#include <gtest/gtest.h>

namespace shipment {
namespace {

TEST(GofStateMachineTest, StartsInCreated) {
    GofStateMachine machine;
    EXPECT_EQ(machine.status(), ShipmentStatus::Created);
}

TEST(GofStateMachineTest, PayShipDeliverWalksTheHappyPath) {
    GofStateMachine machine;

    machine.pay();
    EXPECT_EQ(machine.status(), ShipmentStatus::Paid);

    machine.ship();
    EXPECT_EQ(machine.status(), ShipmentStatus::InTransit);

    machine.deliver();
    EXPECT_EQ(machine.status(), ShipmentStatus::Delivered);
}

TEST(GofStateMachineTest, CancelFromCreatedIsLegal) {
    GofStateMachine machine;
    machine.cancel();
    EXPECT_EQ(machine.status(), ShipmentStatus::Cancelled);
}

TEST(GofStateMachineTest, CancelFromPaidIsLegal) {
    GofStateMachine machine;
    machine.pay();
    machine.cancel();
    EXPECT_EQ(machine.status(), ShipmentStatus::Cancelled);
}

TEST(GofStateMachineTest, ShipBeforePayIsRejected) {
    GofStateMachine machine;

    EXPECT_THROW(machine.ship(), IllegalTransition);
    EXPECT_EQ(machine.status(), ShipmentStatus::Created);
}

TEST(GofStateMachineTest, CancelAfterInTransitIsRejected) {
    GofStateMachine machine;
    machine.pay();
    machine.ship();

    EXPECT_THROW(machine.cancel(), IllegalTransition);
    EXPECT_EQ(machine.status(), ShipmentStatus::InTransit);
}

TEST(GofStateMachineTest, EventsAfterDeliveredAreAllRejected) {
    GofStateMachine machine;
    machine.pay();
    machine.ship();
    machine.deliver();

    EXPECT_THROW(machine.pay(), IllegalTransition);
    EXPECT_THROW(machine.ship(), IllegalTransition);
    EXPECT_THROW(machine.deliver(), IllegalTransition);
    EXPECT_THROW(machine.cancel(), IllegalTransition);
}

}  // namespace
}  // namespace shipment

#include "table_fsm.hpp"

#include <gtest/gtest.h>

namespace shipment {
namespace {

TEST(TableShipmentMachineTest, StartsInCreated) {
    TableShipmentMachine machine;
    EXPECT_EQ(machine.status(), ShipmentStatus::Created);
}

TEST(TableShipmentMachineTest, PayShipDeliverWalksTheHappyPath) {
    TableShipmentMachine machine;

    machine.apply(ShipmentEvent::Pay);
    EXPECT_EQ(machine.status(), ShipmentStatus::Paid);

    machine.apply(ShipmentEvent::Ship);
    EXPECT_EQ(machine.status(), ShipmentStatus::InTransit);

    machine.apply(ShipmentEvent::Deliver);
    EXPECT_EQ(machine.status(), ShipmentStatus::Delivered);
}

TEST(TableShipmentMachineTest, CancelFromCreatedIsLegal) {
    TableShipmentMachine machine;
    machine.apply(ShipmentEvent::Cancel);
    EXPECT_EQ(machine.status(), ShipmentStatus::Cancelled);
}

TEST(TableShipmentMachineTest, CancelFromPaidIsLegal) {
    TableShipmentMachine machine;
    machine.apply(ShipmentEvent::Pay);
    machine.apply(ShipmentEvent::Cancel);
    EXPECT_EQ(machine.status(), ShipmentStatus::Cancelled);
}

TEST(TableShipmentMachineTest, ShipBeforePayIsRejected) {
    TableShipmentMachine machine;

    EXPECT_THROW(machine.apply(ShipmentEvent::Ship), IllegalTransition);
    EXPECT_EQ(machine.status(), ShipmentStatus::Created);
}

TEST(TableShipmentMachineTest, CancelAfterInTransitIsRejected) {
    TableShipmentMachine machine;
    machine.apply(ShipmentEvent::Pay);
    machine.apply(ShipmentEvent::Ship);

    EXPECT_THROW(machine.apply(ShipmentEvent::Cancel), IllegalTransition);
    EXPECT_EQ(machine.status(), ShipmentStatus::InTransit);
}

#if __cplusplus >= 202302L
TEST(TableShipmentMachineTest, TryApplyRejectsAnIllegalEventWithATypedError) {
    TableShipmentMachine machine;

    const auto result = machine.try_apply(ShipmentEvent::Ship);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "cannot apply event 'Ship' to a shipment in status 'Created'");
    EXPECT_EQ(machine.status(), ShipmentStatus::Created);
}

TEST(TableShipmentMachineTest, TryApplyReturnsTheNextStatusForALegalEvent) {
    TableShipmentMachine machine;

    const auto result = machine.try_apply(ShipmentEvent::Pay);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), ShipmentStatus::Paid);
    EXPECT_EQ(machine.status(), ShipmentStatus::Paid);
}
#endif

}  // namespace
}  // namespace shipment

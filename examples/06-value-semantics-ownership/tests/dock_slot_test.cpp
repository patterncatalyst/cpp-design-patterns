#include "dock_slot.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace warehouse {
namespace {

TEST(DockSlotTest, AssignTakesOwnership) {
    DockSlot slot;
    EXPECT_FALSE(slot.occupied());

    slot.assign(std::make_unique<Parcel>(Parcel{"TRK-2001", 1.0, "Standard"}));

    EXPECT_TRUE(slot.occupied());
    EXPECT_EQ(slot.peek()->tracking_id, "TRK-2001");
}

TEST(DockSlotTest, ReassignDestroysThePreviousParcel) {
    DockSlot slot;
    slot.assign(std::make_unique<Parcel>(Parcel{"TRK-OLD", 1.0, "Standard"}));

    slot.assign(std::make_unique<Parcel>(Parcel{"TRK-NEW", 2.0, "Priority"}));

    EXPECT_EQ(slot.peek()->tracking_id, "TRK-NEW");
}

TEST(TransferParcelTest, MoveTransfersOwnershipAndEmptiesTheSource) {
    DockSlot inbound;
    DockSlot outbound;
    inbound.assign(std::make_unique<Parcel>(Parcel{"TRK-2002", 1.8, "Standard"}));

    transfer_parcel(inbound, outbound);

    EXPECT_FALSE(inbound.occupied());
    EXPECT_EQ(inbound.peek(), nullptr);
    ASSERT_TRUE(outbound.occupied());
    EXPECT_EQ(outbound.peek()->tracking_id, "TRK-2002");
}

TEST(TransferParcelTest, TransferringIntoAnOccupiedSlotReplacesItsParcel) {
    DockSlot inbound;
    DockSlot outbound;
    inbound.assign(std::make_unique<Parcel>(Parcel{"TRK-NEW-ARRIVAL", 1.0, "Standard"}));
    outbound.assign(std::make_unique<Parcel>(Parcel{"TRK-ALREADY-THERE", 2.0, "Priority"}));

    transfer_parcel(inbound, outbound);

    EXPECT_FALSE(inbound.occupied());
    ASSERT_TRUE(outbound.occupied());
    EXPECT_EQ(outbound.peek()->tracking_id, "TRK-NEW-ARRIVAL");
}

TEST(DockSlotTest, ReleaseEmptiesTheSlotAndReturnsTheParcel) {
    DockSlot slot;
    slot.assign(std::make_unique<Parcel>(Parcel{"TRK-2003", 1.0, "Standard"}));

    std::unique_ptr<Parcel> released = slot.release();

    EXPECT_FALSE(slot.occupied());
    ASSERT_NE(released, nullptr);
    EXPECT_EQ(released->tracking_id, "TRK-2003");
}

}  // namespace
}  // namespace warehouse

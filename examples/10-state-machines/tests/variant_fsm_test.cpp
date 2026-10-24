#include "variant_fsm.hpp"

#include <gtest/gtest.h>

namespace shipment {
namespace {

TEST(VariantFsmTest, StartsInCreated) {
    const ShipmentVariant state = CreatedTag{};
    EXPECT_EQ(status_of(state), ShipmentStatus::Created);
}

TEST(VariantFsmTest, PayShipDeliverWalksTheHappyPath) {
    ShipmentVariant state = CreatedTag{};

    state = apply_event(state, ShipmentEvent::Pay);
    EXPECT_EQ(status_of(state), ShipmentStatus::Paid);

    state = apply_event(state, ShipmentEvent::Ship);
    EXPECT_EQ(status_of(state), ShipmentStatus::InTransit);

    state = apply_event(state, ShipmentEvent::Deliver);
    EXPECT_EQ(status_of(state), ShipmentStatus::Delivered);
}

TEST(VariantFsmTest, CancelFromCreatedIsLegal) {
    ShipmentVariant state = CreatedTag{};
    state = apply_event(state, ShipmentEvent::Cancel);
    EXPECT_EQ(status_of(state), ShipmentStatus::Cancelled);
}

TEST(VariantFsmTest, CancelFromPaidIsLegal) {
    ShipmentVariant state = CreatedTag{};
    state = apply_event(state, ShipmentEvent::Pay);
    state = apply_event(state, ShipmentEvent::Cancel);
    EXPECT_EQ(status_of(state), ShipmentStatus::Cancelled);
}

TEST(VariantFsmTest, ShipBeforePayIsRejected) {
    const ShipmentVariant state = CreatedTag{};
    EXPECT_THROW(apply_event(state, ShipmentEvent::Ship), IllegalTransition);
}

TEST(VariantFsmTest, CancelAfterInTransitIsRejected) {
    ShipmentVariant state = CreatedTag{};
    state = apply_event(state, ShipmentEvent::Pay);
    state = apply_event(state, ShipmentEvent::Ship);

    EXPECT_THROW(apply_event(state, ShipmentEvent::Cancel), IllegalTransition);
}

}  // namespace
}  // namespace shipment

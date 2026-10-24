#include <gtest/gtest.h>

#include <vector>

#include "gof_state.hpp"
#include "shipment_domain.hpp"
#include "table_fsm.hpp"
#include "variant_fsm.hpp"

namespace shipment {
namespace {

void apply_to_gof(GofStateMachine& machine, ShipmentEvent event) {
    switch (event) {
        case ShipmentEvent::Pay: machine.pay(); return;
        case ShipmentEvent::Ship: machine.ship(); return;
        case ShipmentEvent::Deliver: machine.deliver(); return;
        case ShipmentEvent::Cancel: machine.cancel(); return;
        case ShipmentEvent::Advance: return;
    }
}

TEST(CrossCheckTest, AllThreeDeterministicExpressionsAgreeOnTheHappyPath) {
    const std::vector<ShipmentEvent> events = {ShipmentEvent::Pay, ShipmentEvent::Ship, ShipmentEvent::Deliver};

    GofStateMachine gof;
    TableShipmentMachine table;
    ShipmentVariant variant = CreatedTag{};

    for (const auto event : events) {
        apply_to_gof(gof, event);
        table.apply(event);
        variant = apply_event(variant, event);

        EXPECT_EQ(gof.status(), table.status());
        EXPECT_EQ(table.status(), status_of(variant));
    }
}

TEST(CrossCheckTest, AllThreeDeterministicExpressionsAgreeOnCancelFromPaid) {
    GofStateMachine gof;
    TableShipmentMachine table;
    ShipmentVariant variant = CreatedTag{};

    apply_to_gof(gof, ShipmentEvent::Pay);
    table.apply(ShipmentEvent::Pay);
    variant = apply_event(variant, ShipmentEvent::Pay);

    apply_to_gof(gof, ShipmentEvent::Cancel);
    table.apply(ShipmentEvent::Cancel);
    variant = apply_event(variant, ShipmentEvent::Cancel);

    EXPECT_EQ(gof.status(), ShipmentStatus::Cancelled);
    EXPECT_EQ(table.status(), ShipmentStatus::Cancelled);
    EXPECT_EQ(status_of(variant), ShipmentStatus::Cancelled);
}

TEST(CrossCheckTest, AllThreeDeterministicExpressionsRejectTheSameIllegalEvent) {
    GofStateMachine gof;
    TableShipmentMachine table;
    const ShipmentVariant variant = CreatedTag{};

    EXPECT_THROW(gof.ship(), IllegalTransition);
    EXPECT_THROW(table.apply(ShipmentEvent::Ship), IllegalTransition);
    EXPECT_THROW(apply_event(variant, ShipmentEvent::Ship), IllegalTransition);
}

}  // namespace
}  // namespace shipment

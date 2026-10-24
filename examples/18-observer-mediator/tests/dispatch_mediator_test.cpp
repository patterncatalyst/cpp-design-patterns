#include "dispatch_mediator.hpp"

#include <gtest/gtest.h>

namespace shipping {
namespace {

TEST(DispatchCoordinatorTest, RoutesAWarehouseReadyEventToCarrierPickup) {
    WarehouseStation warehouse;
    CarrierStation carrier;
    CustomerNotificationStation notifier;
    DispatchCoordinator coordinator(warehouse, carrier, notifier);

    warehouse.mark_order_ready("ORD-100");

    ASSERT_EQ(carrier.picked_up_orders().size(), 1u);
    EXPECT_EQ(carrier.picked_up_orders().front(), "ORD-100");
}

TEST(DispatchCoordinatorTest, CascadesAPickupIntoACustomerNotificationWithoutTheCarrierKnowingTheNotifier) {
    WarehouseStation warehouse;
    CarrierStation carrier;
    CustomerNotificationStation notifier;
    DispatchCoordinator coordinator(warehouse, carrier, notifier);

    warehouse.mark_order_ready("ORD-200");

    ASSERT_EQ(notifier.sent_messages().size(), 1u);
    EXPECT_EQ(notifier.sent_messages().front(), "ORD-200: Your package has been picked up and is on its way.");
}

TEST(DispatchCoordinatorTest, RoutesOutForDeliveryAndExceptionEventsToTheirOwnNotifications) {
    WarehouseStation warehouse;
    CarrierStation carrier;
    CustomerNotificationStation notifier;
    DispatchCoordinator coordinator(warehouse, carrier, notifier);

    carrier.mark_out_for_delivery("ORD-300");
    carrier.report_exception("ORD-300", "damaged during transit");

    ASSERT_EQ(notifier.sent_messages().size(), 2u);
    EXPECT_EQ(notifier.sent_messages()[0], "ORD-300: Your package is out for delivery today.");
    EXPECT_EQ(notifier.sent_messages()[1],
              "ORD-300: There is a delivery exception on your package; our team is investigating.");
    ASSERT_EQ(carrier.exception_reasons().size(), 1u);
    EXPECT_EQ(carrier.exception_reasons().front(), "damaged during transit");
}

TEST(DispatchCoordinatorTest, TwoOrdersThroughTheSameCoordinatorStayIndependentlyRouted) {
    WarehouseStation warehouse;
    CarrierStation carrier;
    CustomerNotificationStation notifier;
    DispatchCoordinator coordinator(warehouse, carrier, notifier);

    warehouse.mark_order_ready("ORD-400");
    warehouse.mark_order_ready("ORD-500");

    EXPECT_EQ(carrier.picked_up_orders().size(), 2u);
    EXPECT_EQ(notifier.sent_messages().size(), 2u);
    EXPECT_EQ(carrier.picked_up_orders()[0], "ORD-400");
    EXPECT_EQ(carrier.picked_up_orders()[1], "ORD-500");
}

}  // namespace
}  // namespace shipping

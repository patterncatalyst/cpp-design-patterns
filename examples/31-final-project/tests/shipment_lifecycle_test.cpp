#include "shipment.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "lifecycle_observer.hpp"
#include "shipment_types.hpp"

namespace finalproject31 {
namespace {

using ::testing::_;

class MockLifecycleObserver : public LifecycleObserver {
public:
    MOCK_METHOD(void, on_transitioned, (const Shipment& shipment, ShipmentStatus from, ShipmentStatus to),
                (override));
};

Shipment make_shipment() { return Shipment("SHIP-1", "Groundline Parcel", "LOCAL", 3.0, 899); }

TEST(ShipmentLifecycleTest, StartsCreated) {
    Shipment shipment = make_shipment();
    EXPECT_EQ(shipment.status(), ShipmentStatus::Created);
}

TEST(ShipmentLifecycleTest, LegalEventAdvancesStatusAndNotifiesSubscribedObserver) {
    Shipment shipment = make_shipment();
    MockLifecycleObserver observer;
    shipment.subscribe(observer);

    EXPECT_CALL(observer, on_transitioned(testing::Ref(shipment), ShipmentStatus::Created, ShipmentStatus::PickedUp))
        .Times(1);

    shipment.advance(ShipmentEvent::PickUp);

    EXPECT_EQ(shipment.status(), ShipmentStatus::PickedUp);
}

TEST(ShipmentLifecycleTest, FullSequenceReachesDelivered) {
    Shipment shipment = make_shipment();

    shipment.advance(ShipmentEvent::PickUp);
    shipment.advance(ShipmentEvent::Dispatch);
    shipment.advance(ShipmentEvent::Deliver);

    EXPECT_EQ(shipment.status(), ShipmentStatus::Delivered);
}

TEST(ShipmentLifecycleTest, IllegalEventThrowsAndDoesNotNotifyObserver) {
    Shipment shipment = make_shipment();
    MockLifecycleObserver observer;
    shipment.subscribe(observer);

    EXPECT_CALL(observer, on_transitioned(_, _, _)).Times(0);

    EXPECT_THROW(shipment.advance(ShipmentEvent::Deliver), IllegalTransitionError);
    EXPECT_EQ(shipment.status(), ShipmentStatus::Created);
}

TEST(ShipmentLifecycleTest, EventPastDeliveredIsAlwaysIllegal) {
    Shipment shipment = make_shipment();
    shipment.advance(ShipmentEvent::PickUp);
    shipment.advance(ShipmentEvent::Dispatch);
    shipment.advance(ShipmentEvent::Deliver);

    EXPECT_THROW(shipment.advance(ShipmentEvent::PickUp), IllegalTransitionError);
}

TEST(ShipmentLifecycleTest, UnsubscribedObserverReceivesNoFurtherNotifications) {
    Shipment shipment = make_shipment();
    MockLifecycleObserver observer;
    shipment.subscribe(observer);
    shipment.unsubscribe(observer);

    EXPECT_CALL(observer, on_transitioned(_, _, _)).Times(0);

    shipment.advance(ShipmentEvent::PickUp);
}

}  // namespace
}  // namespace finalproject31

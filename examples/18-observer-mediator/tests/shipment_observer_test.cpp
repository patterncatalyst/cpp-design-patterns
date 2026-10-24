#include "shipment_observer.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace shipping {
namespace {

TEST(ShipmentTrackerTest, NotifiesAnInterfaceBasedObserverOnStatusChange) {
    ShipmentTracker tracker("TRK-9001");
    DashboardView dashboard;
    auto sub = tracker.subscribe(dashboard);

    tracker.advance_status(ShipmentStatus::PickedUp);

    EXPECT_EQ(dashboard.current_status_line(), "TRK-9001: PickedUp");
}

TEST(ShipmentTrackerTest, NotifiesAllSubscribersInSubscriptionOrder) {
    ShipmentTracker tracker("TRK-9002");
    std::vector<std::string> call_order;
    auto sub_a =
        tracker.subscribe([&call_order](const std::string&, ShipmentStatus, ShipmentStatus) { call_order.push_back("a"); });
    auto sub_b =
        tracker.subscribe([&call_order](const std::string&, ShipmentStatus, ShipmentStatus) { call_order.push_back("b"); });
    auto sub_c =
        tracker.subscribe([&call_order](const std::string&, ShipmentStatus, ShipmentStatus) { call_order.push_back("c"); });

    tracker.advance_status(ShipmentStatus::PickedUp);

    EXPECT_EQ(call_order, (std::vector<std::string>{"a", "b", "c"}));
}

TEST(ShipmentTrackerTest, DoesNotNotifyWhenTheStatusIsUnchanged) {
    ShipmentTracker tracker("TRK-9003");
    int call_count = 0;
    auto sub = tracker.subscribe([&call_count](const std::string&, ShipmentStatus, ShipmentStatus) { ++call_count; });

    tracker.advance_status(ShipmentStatus::Created);

    EXPECT_EQ(call_count, 0);
}

TEST(ShipmentTrackerTest, StopsNotifyingAfterTheSubscriptionIsReleased) {
    ShipmentTracker tracker("TRK-9004");
    int call_count = 0;
    {
        auto sub = tracker.subscribe([&call_count](const std::string&, ShipmentStatus, ShipmentStatus) { ++call_count; });
        tracker.advance_status(ShipmentStatus::PickedUp);
    }  // sub destructed here: unsubscribes before anything else happens.

    tracker.advance_status(ShipmentStatus::InTransit);

    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(tracker.subscriber_count(), 0u);
}

TEST(ShipmentTrackerTest, LightweightCallbackSubscriberNeedsNoObserverClass) {
    ShipmentTracker tracker("TRK-9005");
    std::vector<std::string> audit_entries;
    auto sub = tracker.subscribe(
        [&audit_entries](const std::string& tracking_id, ShipmentStatus old_status, ShipmentStatus new_status) {
            audit_entries.push_back(tracking_id + ": " + to_string(old_status) + " -> " + to_string(new_status));
        });

    tracker.advance_status(ShipmentStatus::PickedUp);
    tracker.advance_status(ShipmentStatus::InTransit);

    ASSERT_EQ(audit_entries.size(), 2u);
    EXPECT_EQ(audit_entries[0], "TRK-9005: Created -> PickedUp");
    EXPECT_EQ(audit_entries[1], "TRK-9005: PickedUp -> InTransit");
}

TEST(AlertingServiceTest, RaisesAnAlertOnlyWhenStatusBecomesException) {
    ShipmentTracker tracker("TRK-9006");
    AlertingService alerting;
    auto sub = tracker.subscribe(alerting);

    tracker.advance_status(ShipmentStatus::PickedUp);
    tracker.advance_status(ShipmentStatus::Exception);

    ASSERT_EQ(alerting.alerts().size(), 1u);
    EXPECT_EQ(alerting.alerts().front(), "TRK-9006 flagged: PickedUp -> Exception");
}

TEST(ShipmentTrackerTest, SubscribingAfterATerminalStatusThrows) {
    ShipmentTracker tracker("TRK-9007");
    tracker.advance_status(ShipmentStatus::Delivered);

    EXPECT_THROW(tracker.subscribe([](const std::string&, ShipmentStatus, ShipmentStatus) {}), std::logic_error);
}

#if __cplusplus >= 202302L
TEST(ShipmentTrackerTest, TrySubscribeReturnsATypedErrorAfterATerminalStatus) {
    ShipmentTracker tracker("TRK-9008");
    tracker.advance_status(ShipmentStatus::Exception);

    const auto result = tracker.try_subscribe([](const std::string&, ShipmentStatus, ShipmentStatus) {});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "cannot subscribe to a shipment that already reached a terminal status");
}

TEST(ShipmentTrackerTest, TrySubscribeSucceedsBeforeATerminalStatus) {
    ShipmentTracker tracker("TRK-9009");

    auto result = tracker.try_subscribe([](const std::string&, ShipmentStatus, ShipmentStatus) {});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(tracker.subscriber_count(), 1u);
}
#endif

// An observer that owns its own subscription, declared so the subscription
// (declared second) is destroyed before the observer (declared first).
// Member destruction order is reverse declaration order, so this ordering
// guarantees the tracker's list is cleaned up before dashboard_ itself is
// torn down: the exact ordering this chapter's Observer section relies on
// to avoid notifying into freed memory.
class ScopedDashboardObserver {
public:
    explicit ScopedDashboardObserver(ShipmentTracker& tracker) : subscription_(tracker.subscribe(dashboard_)) {}

    DashboardView dashboard_;

private:
    ScopedSubscription subscription_;
};

TEST(ShipmentTrackerTest, AnObserverThatOwnsItsSubscriptionUnsubscribesBeforeItIsDestroyed) {
    ShipmentTracker tracker("TRK-9010");
    {
        ScopedDashboardObserver owned(tracker);
        tracker.advance_status(ShipmentStatus::PickedUp);
        EXPECT_EQ(tracker.subscriber_count(), 1u);
        EXPECT_EQ(owned.dashboard_.current_status_line(), "TRK-9010: PickedUp");
    }  // subscription_ destructs before dashboard_: unsubscribe runs first.

    EXPECT_EQ(tracker.subscriber_count(), 0u);
    // If the subscription had outlived the observer, this call would
    // notify into a destroyed DashboardView. It does not, because the
    // tracker's subscriber list is already empty by this point.
    tracker.advance_status(ShipmentStatus::InTransit);
}

}  // namespace
}  // namespace shipping

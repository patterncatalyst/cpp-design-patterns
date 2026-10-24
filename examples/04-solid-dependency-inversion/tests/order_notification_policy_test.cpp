#include "order_notification_policy.hpp"

#include <gtest/gtest.h>

#include "fakes.hpp"

namespace notify {
namespace {

TEST(OrderNotificationPolicyTest, DelegatesToTheInjectedAbstractionNotAConcreteType) {
    FakeMessageSender sender;
    OrderNotificationPolicy policy(sender);
    const OrderEvent event{"ORD-1", "customer@example.com", "Your order has shipped."};

    const DispatchResult result = policy.notify_customer(event);

    EXPECT_EQ(result.status, DispatchStatus::Sent);
    ASSERT_EQ(sender.sent_events().size(), 1U);
    EXPECT_EQ(sender.sent_events().front().order_id, "ORD-1");
}

TEST(OrderNotificationPolicyTest, PassesTheEventThroughUnchanged) {
    FakeMessageSender sender;
    OrderNotificationPolicy policy(sender);
    const OrderEvent event{"ORD-2", "+1-555-0101", "Delivery is delayed one day."};

    policy.notify_customer(event);

    ASSERT_EQ(sender.sent_events().size(), 1U);
    EXPECT_EQ(sender.sent_events().front().customer_contact, "+1-555-0101");
    EXPECT_EQ(sender.sent_events().front().message_body, "Delivery is delayed one day.");
}

#if __cplusplus >= 202302L
TEST(OrderNotificationPolicyTest, TryNotifyCustomerRejectsAMissingContactUnderCpp23) {
    FakeMessageSender sender;
    OrderNotificationPolicy policy(sender);
    const OrderEvent uncontactable{"ORD-3", "", "no contact on file"};

    const auto result = policy.try_notify_customer(uncontactable);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "order event has no customer_contact to notify");
    EXPECT_TRUE(sender.sent_events().empty());
}

TEST(OrderNotificationPolicyTest, TryNotifyCustomerSendsAValidEvent) {
    FakeMessageSender sender;
    OrderNotificationPolicy policy(sender);
    const OrderEvent event{"ORD-4", "customer@example.com", "Your order has shipped."};

    const auto result = policy.try_notify_customer(event);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, DispatchStatus::Sent);
}
#else
TEST(OrderNotificationPolicyTest, TryNotifyCustomerRejectsAMissingContactUnderCpp20) {
    FakeMessageSender sender;
    OrderNotificationPolicy policy(sender);
    const OrderEvent uncontactable{"ORD-3", "", "no contact on file"};

    const auto result = policy.try_notify_customer(uncontactable);

    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(sender.sent_events().empty());
}

TEST(OrderNotificationPolicyTest, TryNotifyCustomerSendsAValidEvent) {
    FakeMessageSender sender;
    OrderNotificationPolicy policy(sender);
    const OrderEvent event{"ORD-4", "customer@example.com", "Your order has shipped."};

    const auto result = policy.try_notify_customer(event);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, DispatchStatus::Sent);
}
#endif

}  // namespace
}  // namespace notify

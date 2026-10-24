#include "templated_order_notifier.hpp"

#include <gtest/gtest.h>

#include "fakes.hpp"

namespace notify {
namespace {

TEST(TemplatedOrderNotifierTest, RendersBeforeSendingAndDependsOnlyOnTheTwoRoleInterfaces) {
    FakeTemplateRenderer renderer;
    FakeMessageSender sender;
    TemplatedOrderNotifier notifier(renderer, sender, "order-delayed");
    const OrderEvent event{"ORD-5", "customer@example.com", "original body, replaced by the template"};

    notifier.notify_customer(event);

    ASSERT_EQ(sender.sent_events().size(), 1U);
    EXPECT_EQ(sender.sent_events().front().message_body, "order-delayed:ORD-5");
}

TEST(TemplatedOrderNotifierTest, LeavesOrderIdAndContactUntouched) {
    FakeTemplateRenderer renderer;
    FakeMessageSender sender;
    TemplatedOrderNotifier notifier(renderer, sender, "order-delayed");
    const OrderEvent event{"ORD-6", "+1-555-0102", "original body"};

    notifier.notify_customer(event);

    ASSERT_EQ(sender.sent_events().size(), 1U);
    EXPECT_EQ(sender.sent_events().front().order_id, "ORD-6");
    EXPECT_EQ(sender.sent_events().front().customer_contact, "+1-555-0102");
}

}  // namespace
}  // namespace notify

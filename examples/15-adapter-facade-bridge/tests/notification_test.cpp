#include "notification.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace shipping {
namespace {

TEST(NotificationTest, ShipmentCreatedFormatsAndDeliversThroughEmail) {
    auto transport = std::make_shared<EmailTransport>();
    ShipmentCreatedNotification notification(transport, "TRK-9001");

    const std::string result = notification.send("customer@example.com");

    EXPECT_EQ(result, "EMAIL to customer@example.com: Your shipment TRK-9001 has been created.");
}

TEST(NotificationTest, SameNotificationKindOverSmsUsesTheSameMessageADifferentTransport) {
    auto email = std::make_shared<EmailTransport>();
    auto sms = std::make_shared<SmsTransport>();
    ShipmentCreatedNotification over_email(email, "TRK-9001");
    ShipmentCreatedNotification over_sms(sms, "TRK-9001");

    const std::string email_result = over_email.send("customer@example.com");
    const std::string sms_result = over_sms.send("+1-555-0100");

    EXPECT_EQ(email_result, "EMAIL to customer@example.com: Your shipment TRK-9001 has been created.");
    EXPECT_EQ(sms_result, "SMS to +1-555-0100: Your shipment TRK-9001 has been created.");
}

TEST(NotificationTest, DeliveryDelayedFormatsItsOwnMessageOverTheSameTransportType) {
    auto transport = std::make_shared<SmsTransport>();
    DeliveryDelayedNotification notification(transport, "TRK-9001", /*delay_hours=*/6);

    const std::string result = notification.send("+1-555-0100");

    EXPECT_EQ(result, "SMS to +1-555-0100: Shipment TRK-9001 is delayed by 6 hours.");
}

TEST(NotificationTest, TwoKindsAndTwoTransportsComposeWithoutAnExtraClassPerCombination) {
    // Two refined Notification kinds and two NotificationTransport
    // implementations produce all four combinations from the two existing
    // hierarchies; no ShipmentCreatedEmailNotification or
    // DeliveryDelayedSmsNotification class exists anywhere in this example.
    auto email = std::make_shared<EmailTransport>();
    auto sms = std::make_shared<SmsTransport>();

    ShipmentCreatedNotification created_email(email, "TRK-1");
    ShipmentCreatedNotification created_sms(sms, "TRK-1");
    DeliveryDelayedNotification delayed_email(email, "TRK-1", /*delay_hours=*/2);
    DeliveryDelayedNotification delayed_sms(sms, "TRK-1", /*delay_hours=*/2);

    EXPECT_NE(created_email.send("a@example.com"), created_sms.send("+1"));
    EXPECT_NE(delayed_email.send("a@example.com"), delayed_sms.send("+1"));
    EXPECT_NE(created_email.send("a@example.com"), delayed_email.send("a@example.com"));
}

}  // namespace
}  // namespace shipping

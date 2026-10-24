#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "email_gateway.hpp"
#include "gateway_interfaces.hpp"
#include "notification_domain.hpp"
#include "sms_gateway.hpp"

namespace notify {
namespace {

// Compile-time proof of the Interface Segregation payoff: SmsGateway was
// never asked to implement TemplateRenderer or DeliveryStatusProvider,
// because it never declares them as a base. If a fat "gateway" interface
// bundled all three capabilities, this would be a static_assert failure
// instead of a static_assert success, because SmsGateway would be forced
// to inherit (and stub out) methods it cannot honestly implement.
static_assert(std::is_base_of_v<MessageSender, SmsGateway>);
static_assert(!std::is_base_of_v<TemplateRenderer, SmsGateway>);
static_assert(!std::is_base_of_v<DeliveryStatusProvider, SmsGateway>);

static_assert(std::is_base_of_v<MessageSender, EmailGateway>);
static_assert(std::is_base_of_v<TemplateRenderer, EmailGateway>);
static_assert(std::is_base_of_v<DeliveryStatusProvider, EmailGateway>);

TEST(EmailGatewayTest, SendRecordsADispatchThatStatusOfCanLookUp) {
    EmailGateway gateway;
    const OrderEvent event{"ORD-10", "customer@example.com", "Your order has shipped."};

    const DispatchResult result = gateway.send(event);

    EXPECT_EQ(result.status, DispatchStatus::Sent);
    EXPECT_EQ(gateway.status_of(result.dispatch_id), DispatchStatus::Sent);
}

TEST(EmailGatewayTest, StatusOfAnUnknownDispatchIdIsFailed) {
    EmailGateway gateway;

    EXPECT_EQ(gateway.status_of("never-sent"), DispatchStatus::Failed);
}

TEST(EmailGatewayTest, RenderSubstitutesTheMessageBodyIntoTheNamedTemplate) {
    EmailGateway gateway;
    const OrderEvent event{"ORD-11", "customer@example.com", "it left the warehouse this morning"};

    const std::string rendered = gateway.render("order-delayed", event);

    EXPECT_EQ(rendered, "We're sorry, your order is running late. it left the warehouse this morning");
}

TEST(EmailGatewayTest, SendUsesTheSharedFormatMessageHelper) {
    EmailGateway gateway;
    const OrderEvent event{"ORD-12", "customer@example.com", "Your order has shipped."};

    const DispatchResult result = gateway.send(event);

    EXPECT_EQ(gateway.body_sent_for(result.dispatch_id), format_message(event));
}

TEST(SmsGatewayTest, SendUsesTheSameSharedFormatMessageHelperAsEmail) {
    SmsGateway gateway;
    const OrderEvent event{"ORD-13", "+1-555-0103", "Your order has shipped."};

    gateway.send(event);

    EXPECT_EQ(gateway.last_sent_body(), format_message(event));
}

TEST(SmsGatewayTest, TruncatesAMessageLongerThanOneHundredSixtyCharacters) {
    SmsGateway gateway;
    const OrderEvent event{"ORD-14", "+1-555-0104", std::string(200, 'x')};

    gateway.send(event);

    EXPECT_EQ(gateway.last_sent_body().size(), 160U);
}

}  // namespace
}  // namespace notify

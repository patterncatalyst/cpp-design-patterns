#include "shipment_alert_notifier.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace breakdep28 {
namespace {

using ::testing::HasSubstr;

class MockAlertChannel : public AlertChannel {
public:
    MOCK_METHOD(void, send, (const std::string& recipient, const std::string& message), (const, override));
};

class RecordingAlertChannel : public AlertChannel {
public:
    void send(const std::string& recipient, const std::string& message) const override {
        sent_.emplace_back(recipient, message);
    }

    const std::vector<std::pair<std::string, std::string>>& sent() const { return sent_; }

private:
    mutable std::vector<std::pair<std::string, std::string>> sent_;
};

TEST(ShipmentAlertNotifierTest, DefaultConstructorStillWorksWithTheRealChannel) {
    // Parameterize Constructor's whole promise: this call site predates the
    // technique and still compiles, unchanged, against the real
    // SmtpAlertSender the default argument constructs.
    ShipmentAlertNotifier notifier("supervisor@example.com");

    EXPECT_NO_THROW(notifier.flag_for_manual_review("SH-7001", "weight mismatch"));
}

TEST(ShipmentAlertNotifierTest, InjectedMockVerifiesTheInteraction) {
    MockAlertChannel channel;
    EXPECT_CALL(channel, send("supervisor@example.com", HasSubstr("SH-7002"))).Times(1);

    // A non-owning shared_ptr with a no-op deleter: the mock lives on the
    // stack, and ShipmentAlertNotifier never needs to know that. Extract
    // Interface is what makes this object acceptable here at all; the
    // inherited constructor could only ever have taken a real
    // SmtpAlertSender.
    ShipmentAlertNotifier notifier("supervisor@example.com", std::shared_ptr<AlertChannel>(&channel, [](AlertChannel*) {}));

    notifier.flag_for_manual_review("SH-7002", "address undeliverable");
}

TEST(ShipmentAlertNotifierTest, InjectedFakeRecordsTheFormattedMessage) {
    auto channel = std::make_shared<RecordingAlertChannel>();
    ShipmentAlertNotifier notifier("supervisor@example.com", channel);

    notifier.flag_for_manual_review("SH-7003", "customs hold");

    ASSERT_EQ(channel->sent().size(), 1u);
    EXPECT_EQ(channel->sent()[0].first, "supervisor@example.com");
    EXPECT_THAT(channel->sent()[0].second, HasSubstr("SH-7003"));
    EXPECT_THAT(channel->sent()[0].second, HasSubstr("customs hold"));
}

}  // namespace
}  // namespace breakdep28

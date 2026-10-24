#include "delivery_auditor.hpp"

#include <gtest/gtest.h>

#include "fakes.hpp"

namespace notify {
namespace {

TEST(DeliveryAuditorTest, ConfirmsADispatchReportedAsSent) {
    FakeDeliveryStatusProvider status_provider(DispatchStatus::Sent);
    DeliveryAuditor auditor(status_provider);

    EXPECT_TRUE(auditor.confirmed_delivered("any-dispatch-id"));
}

TEST(DeliveryAuditorTest, DoesNotConfirmADispatchReportedAsFailed) {
    FakeDeliveryStatusProvider status_provider(DispatchStatus::Failed);
    DeliveryAuditor auditor(status_provider);

    EXPECT_FALSE(auditor.confirmed_delivered("any-dispatch-id"));
}

}  // namespace
}  // namespace notify

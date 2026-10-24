#include "approval_chain.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace shipping {
namespace {

TEST(ApprovalChainTest, StopsAtWeightLimitHandlerForAnOverweightShipment) {
    auto chain = build_default_chain();
    std::vector<std::string> call_log;
    const ShipmentRequest request{/*weight_kg=*/55.0, /*hazmat=*/false, "US"};

    const ApprovalDecision decision = chain->handle(request, call_log);

    EXPECT_EQ(decision.outcome, ApprovalOutcome::Rejected);
    EXPECT_EQ(decision.handler_name, "WeightLimitHandler");
    EXPECT_EQ(call_log, (std::vector<std::string>{"WeightLimitHandler"}));
}

TEST(ApprovalChainTest, StopsAtHazmatHandlerForAnUnclearedHazmatDestination) {
    auto chain = build_default_chain();
    std::vector<std::string> call_log;
    const ShipmentRequest request{/*weight_kg=*/10.0, /*hazmat=*/true, "FR"};

    const ApprovalDecision decision = chain->handle(request, call_log);

    EXPECT_EQ(decision.outcome, ApprovalOutcome::Rejected);
    EXPECT_EQ(decision.handler_name, "HazmatHandler");
    EXPECT_EQ(call_log, (std::vector<std::string>{"WeightLimitHandler", "HazmatHandler"}));
}

TEST(ApprovalChainTest, StopsAtEmbargoHandlerForAnEmbargoedDestination) {
    auto chain = build_default_chain();
    std::vector<std::string> call_log;
    const ShipmentRequest request{/*weight_kg=*/10.0, /*hazmat=*/false, "KP"};

    const ApprovalDecision decision = chain->handle(request, call_log);

    EXPECT_EQ(decision.outcome, ApprovalOutcome::Rejected);
    EXPECT_EQ(decision.handler_name, "EmbargoHandler");
    EXPECT_EQ(call_log, (std::vector<std::string>{"WeightLimitHandler", "HazmatHandler", "EmbargoHandler"}));
}

TEST(ApprovalChainTest, RoutesAnUnhandledRequestToTheManualApprovalFallback) {
    auto chain = build_default_chain();
    std::vector<std::string> call_log;
    const ShipmentRequest request{/*weight_kg=*/10.0, /*hazmat=*/false, "DE"};

    const ApprovalDecision decision = chain->handle(request, call_log);

    EXPECT_EQ(decision.outcome, ApprovalOutcome::NeedsManualReview);
    EXPECT_EQ(decision.handler_name, "ManualApprovalHandler");
    EXPECT_EQ(call_log, (std::vector<std::string>{"WeightLimitHandler", "HazmatHandler", "EmbargoHandler",
                                                   "ManualApprovalHandler"}));
}

}  // namespace
}  // namespace shipping

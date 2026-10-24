#include "factory_method.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace carrier_integration {
namespace {

TEST(SandboxOnboarderTest, PassesWithoutRequiringAWebhook) {
    const SandboxOnboarder onboarder;
    const CarrierAccount account{"fastship", "acct-77", /*webhook_registered=*/false};

    const OnboardingResult result = onboarder.onboard(account);

    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.log.size(), 3u);
}

TEST(ProductionOnboarderTest, PassesWhenTheWebhookIsRegistered) {
    const ProductionOnboarder onboarder;
    const CarrierAccount account{"fastship", "acct-77", /*webhook_registered=*/true};

    const OnboardingResult result = onboarder.onboard(account);

    EXPECT_TRUE(result.passed);
}

TEST(ProductionOnboarderTest, FailsWhenTheWebhookIsNotRegistered) {
    const ProductionOnboarder onboarder;
    const CarrierAccount account{"fastship", "acct-77", /*webhook_registered=*/false};

    const OnboardingResult result = onboarder.onboard(account);

    EXPECT_FALSE(result.passed);
}

TEST(CarrierOnboarderTest, RejectsAnAccountWithNoIdBeforeCreatingAnyTestSuite) {
    const SandboxOnboarder onboarder;
    const CarrierAccount account{"fastship", /*account_id=*/"", /*webhook_registered=*/false};

    EXPECT_THROW(onboarder.onboard(account), std::invalid_argument);
}

TEST(CarrierOnboarderTest, SandboxAndProductionReturnTheSameFixedSequenceShapeForDifferentProducts) {
    // Both subclasses share the exact same onboard() sequence inherited
    // from CarrierOnboarder; only the product create_test_suite() returns
    // differs, which shows up here as a different first log line.
    const SandboxOnboarder sandbox_onboarder;
    const ProductionOnboarder production_onboarder;
    const CarrierAccount account{"fastship", "acct-77", /*webhook_registered=*/true};

    const OnboardingResult sandbox_result = sandbox_onboarder.onboard(account);
    const OnboardingResult production_result = production_onboarder.onboard(account);

    ASSERT_FALSE(sandbox_result.log.empty());
    ASSERT_FALSE(production_result.log.empty());
    EXPECT_NE(sandbox_result.log.front().find("sandbox:"), std::string::npos);
    EXPECT_NE(production_result.log.front().find("production:"), std::string::npos);
}

}  // namespace
}  // namespace carrier_integration

#include "carrier_policy.hpp"

#include <gtest/gtest.h>

namespace exprtmpl {
namespace {

TEST(CarrierPolicyRegistryTest, ResolvesAPolicyRegisteredOnlyInItsOwnTranslationUnit) {
    // WeightTieredCarrierPolicy is defined in weight_tiered_carrier_policy.cpp,
    // in an anonymous namespace, with no declaration anywhere this test can
    // see. The only way this test reaches it at all is through the
    // registration side effect that ran before main(), via the
    // "weight-tiered" key, which is exactly the capability a self-registering
    // registry claims.
    const auto policy = CarrierPolicyRegistry::instance().create("weight-tiered");

    ASSERT_NE(policy, nullptr);
    EXPECT_EQ(policy->name(), "weight-tiered");
}

TEST(CarrierPolicyRegistryTest, EveryRegisteredPolicyClonesIntoAnIndependentInstance) {
    const auto first = CarrierPolicyRegistry::instance().create("flat-rate");
    const auto second = CarrierPolicyRegistry::instance().create("flat-rate");

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first.get(), second.get());
    EXPECT_EQ(first->base_rate_multiplier(), second->base_rate_multiplier());
}

TEST(CarrierPolicyRegistryTest, UnknownKeyResolvesToNoPrototype) {
    const auto policy = CarrierPolicyRegistry::instance().create("overnight-air");

    EXPECT_EQ(policy, nullptr);
}

TEST(CarrierPolicyRegistryTest, AllThreeCarriersSelfRegisteredBeforeMain) {
    // No test here, and no production code anywhere in this example, ever
    // calls register_prototype() directly. All three entries arrived purely
    // from the registrar globals in flat_rate_carrier_policy.cpp,
    // weight_tiered_carrier_policy.cpp, and regional_express_carrier_policy.cpp.
    EXPECT_GE(CarrierPolicyRegistry::instance().size(), 3u);
}

#if __cplusplus >= 202302L
TEST(CarrierPolicyRegistryTest, TryCreateReportsWhyAnEmptyKeyFailsUnderCpp23) {
    const auto result = CarrierPolicyRegistry::instance().try_create("");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "carrier policy key must not be empty");
}

TEST(CarrierPolicyRegistryTest, TryCreateReportsWhyAnUnknownKeyFailsUnderCpp23) {
    const auto result = CarrierPolicyRegistry::instance().try_create("overnight-air");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "no carrier policy registered for key \"overnight-air\"");
}
#endif

}  // namespace
}  // namespace exprtmpl

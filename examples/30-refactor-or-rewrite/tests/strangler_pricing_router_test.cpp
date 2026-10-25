#include "legacy_pricing_engine.hpp"
#include "modern_pricing_engine.hpp"
#include "pricing_characterization_cases.hpp"
#include "strangler_pricing_router.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace strangler30 {
namespace {

// A stand-in for ModernPricingEngine mid-rewrite, with a deliberate bug,
// used only to prove Compare mode catches a disagreement before any
// customer could see it.
class BuggyModernEngine : public PricingEngine {
public:
    Quote quote(const ShipmentRequest&) const override {
        return Quote{999'999, "buggy stand-in"};
    }
};

TEST(StranglerPricingRouterTest, LegacyOnlyModeAlwaysUsesLegacy) {
    auto legacy = std::make_shared<LegacyPricingEngine>();
    auto modern = std::make_shared<ModernPricingEngine>();
    StranglerPricingRouter router(legacy, modern, CutoverPolicy{});
    router.set_mode(StranglerPricingRouter::Mode::LegacyOnly);

    const ShipmentRequest request{"WIDGETCO", "NE", "NE", 2.0, 10'000};

    EXPECT_EQ(router.quote(request).total_cents, legacy->quote(request).total_cents);
}

TEST(StranglerPricingRouterTest, ModernOnlyModeAlwaysUsesModern) {
    auto legacy = std::make_shared<LegacyPricingEngine>();
    auto modern = std::make_shared<ModernPricingEngine>();
    StranglerPricingRouter router(legacy, modern, CutoverPolicy{});
    router.set_mode(StranglerPricingRouter::Mode::ModernOnly);

    const ShipmentRequest request{"GLOBEX", "MW", "MW", 4.0, 15'000};

    EXPECT_EQ(router.quote(request).total_cents, modern->quote(request).total_cents);
}

TEST(StranglerPricingRouterTest, RoutedModeSendsOnlyMigratedCustomersToModern) {
    auto legacy = std::make_shared<LegacyPricingEngine>();
    auto modern = std::make_shared<ModernPricingEngine>();
    CutoverPolicy policy;
    policy.migrate_customer("ACME-2019");
    StranglerPricingRouter router(legacy, modern, policy);
    router.set_mode(StranglerPricingRouter::Mode::Routed);

    const ShipmentRequest migrated{"ACME-2019", "NE", "PAC", 20.0, 50'000};
    const ShipmentRequest not_migrated{"WIDGETCO", "NE", "PAC", 20.0, 50'000};

    EXPECT_EQ(router.quote(migrated).total_cents, modern->quote(migrated).total_cents);
    EXPECT_EQ(router.quote(not_migrated).total_cents, legacy->quote(not_migrated).total_cents);
    EXPECT_EQ(router.policy().migrated_count(), 1U);
}

TEST(StranglerPricingRouterTest, CompareModeFindsNoDisagreementBetweenTheRealEngines) {
    auto legacy = std::make_shared<LegacyPricingEngine>();
    auto modern = std::make_shared<ModernPricingEngine>();
    StranglerPricingRouter router(legacy, modern, CutoverPolicy{});
    router.set_mode(StranglerPricingRouter::Mode::Compare);

    for (const auto& test_case : pricing_characterization_cases()) {
        router.quote(test_case.request);
    }

    EXPECT_TRUE(router.disagreements().empty());
}

TEST(StranglerPricingRouterTest, CompareModeCatchesADisagreementAndStillReturnsLegacysAnswer) {
    auto legacy = std::make_shared<LegacyPricingEngine>();
    auto buggy_modern = std::make_shared<BuggyModernEngine>();
    StranglerPricingRouter router(legacy, buggy_modern, CutoverPolicy{});
    router.set_mode(StranglerPricingRouter::Mode::Compare);

    const ShipmentRequest request{"WIDGETCO", "NE", "NE", 2.0, 10'000};
    const Quote result = router.quote(request);

    EXPECT_EQ(result.total_cents, legacy->quote(request).total_cents);
    ASSERT_EQ(router.disagreements().size(), 1U);
    EXPECT_EQ(router.disagreements().front().modern_quote.total_cents, 999'999);
}

}  // namespace
}  // namespace strangler30

#include "legacy_pricing_engine.hpp"
#include "modern_pricing_engine.hpp"
#include "pricing_characterization_cases.hpp"

#include <gtest/gtest.h>

namespace strangler30 {
namespace {

// The test that makes cutover safe: before StranglerPricingRouter is ever
// allowed to route a single customer to ModernPricingEngine, both engines
// must agree on every case in the shared characterization table. This is
// a direct comparison of the two engines' outputs, independent of the
// pinned expected_total_cents values each per-engine test already checks.
TEST(LegacyModernAgreementTest, BothEnginesAgreeOnEveryCharacterizationCase) {
    LegacyPricingEngine legacy;
    ModernPricingEngine modern;

    for (const auto& test_case : pricing_characterization_cases()) {
        const Quote legacy_quote = legacy.quote(test_case.request);
        const Quote modern_quote = modern.quote(test_case.request);
        EXPECT_EQ(legacy_quote.total_cents, modern_quote.total_cents) << "case: " << test_case.name;
    }
}

}  // namespace
}  // namespace strangler30

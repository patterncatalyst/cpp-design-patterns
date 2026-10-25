#include "modern_pricing_engine.hpp"
#include "pricing_characterization_cases.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace strangler30 {
namespace {

// The point of this file: ModernPricingEngine is judged by the exact same
// table LegacyPricingEngineTest uses, not by a separate set of
// expectations someone wrote while rebuilding the class. A new
// implementation that passes a different suite has not proven anything
// about replacing the old one.
TEST(ModernPricingEngineTest, MatchesEveryCharacterizationCase) {
    ModernPricingEngine engine;
    for (const auto& test_case : pricing_characterization_cases()) {
        EXPECT_EQ(engine.quote(test_case.request).total_cents, test_case.expected_total_cents)
            << "case: " << test_case.name;
    }
}

TEST(ModernPricingEngineTest, RejectsNegativeWeight) {
    ModernPricingEngine engine;
    const ShipmentRequest malformed{"WIDGETCO", "NE", "NE", -1.0, 0};

    EXPECT_THROW(engine.quote(malformed), std::invalid_argument);
}

TEST(ModernPricingEngineTest, RejectsAnUnrecognizedZonePair) {
    ModernPricingEngine engine;
    const ShipmentRequest unknown_zone{"WIDGETCO", "NE", "ANTARCTICA", 1.0, 0};

    EXPECT_THROW(engine.quote(unknown_zone), std::invalid_argument);
}

}  // namespace
}  // namespace strangler30

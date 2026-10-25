#include "legacy_pricing_engine.hpp"
#include "pricing_characterization_cases.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace strangler30 {
namespace {

TEST(LegacyPricingEngineTest, MatchesEveryCharacterizationCase) {
    LegacyPricingEngine engine;
    for (const auto& test_case : pricing_characterization_cases()) {
        EXPECT_EQ(engine.quote(test_case.request).total_cents, test_case.expected_total_cents)
            << "case: " << test_case.name;
    }
}

TEST(LegacyPricingEngineTest, RejectsNegativeWeight) {
    LegacyPricingEngine engine;
    const ShipmentRequest malformed{"WIDGETCO", "NE", "NE", -1.0, 0};

    EXPECT_THROW(engine.quote(malformed), std::invalid_argument);
}

TEST(LegacyPricingEngineTest, RejectsAnUnrecognizedZonePair) {
    LegacyPricingEngine engine;
    const ShipmentRequest unknown_zone{"WIDGETCO", "NE", "ANTARCTICA", 1.0, 0};

    EXPECT_THROW(engine.quote(unknown_zone), std::invalid_argument);
}

#if __cplusplus >= 202302L
TEST(LegacyPricingEngineTest, TryQuoteReturnsATypedErrorForNegativeWeightUnderCpp23) {
    LegacyPricingEngine engine;
    const ShipmentRequest malformed{"WIDGETCO", "NE", "NE", -1.0, 0};

    const auto result = engine.try_quote(malformed);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PricingError::NegativeWeight);
}

TEST(LegacyPricingEngineTest, TryQuoteReturnsTheQuoteForAValidShipmentUnderCpp23) {
    LegacyPricingEngine engine;
    const ShipmentRequest shipment{"WIDGETCO", "NE", "NE", 2.0, 10'000};

    const auto result = engine.try_quote(shipment);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->total_cents, 500);
}
#endif

}  // namespace
}  // namespace strangler30

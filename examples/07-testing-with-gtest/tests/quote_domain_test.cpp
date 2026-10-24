#include "quote_domain.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace freight {
namespace {

// A table-driven test: one ShipmentRequest and the ValidationOutcome it
// must produce. Adding a sixth intake rule later means adding a row here,
// not a new TEST function.
struct ValidationCase {
    std::string name;
    ShipmentRequest request;
    ValidationOutcome expected;
};

class ShipmentValidationTest : public ::testing::TestWithParam<ValidationCase> {};

TEST_P(ShipmentValidationTest, MatchesExpectedOutcome) {
    const ValidationCase& test_case = GetParam();

    EXPECT_EQ(validate_shipment(test_case.request), test_case.expected)
        << "case \"" << test_case.name << "\" did not resolve as expected";
}

INSTANTIATE_TEST_SUITE_P(
    IntakeRules, ShipmentValidationTest,
    ::testing::ValuesIn(std::vector<ValidationCase>{
        {"valid_regional_shipment", ShipmentRequest{3.5, ServiceZone::Regional, 12'000},
         ValidationOutcome::Accepted},
        {"zero_weight_is_rejected", ShipmentRequest{0.0, ServiceZone::Local, 500},
         ValidationOutcome::RejectedNegativeWeight},
        {"negative_weight_is_rejected", ShipmentRequest{-2.0, ServiceZone::National, 500},
         ValidationOutcome::RejectedNegativeWeight},
        {"unassigned_zone_is_rejected", ShipmentRequest{1.0, ServiceZone::Unassigned, 500},
         ValidationOutcome::RejectedMissingZone},
        {"declared_value_over_the_limit_is_rejected",
         ShipmentRequest{1.0, ServiceZone::International, 10'000'000},
         ValidationOutcome::RejectedDeclaredValueTooHigh},
        {"declared_value_at_the_limit_is_accepted",
         ShipmentRequest{1.0, ServiceZone::International, kMaxInsurableValueCents},
         ValidationOutcome::Accepted},
    }),
    [](const ::testing::TestParamInfo<ValidationCase>& info) { return info.param.name; });

// quote_cost() has a precondition its caller must satisfy: weight_kg is
// already non-negative, because validate_shipment() checked it first.
// Calling it directly with a malformed request, bypassing validation the
// way a careless caller might, must abort rather than return a nonsense
// price. FREIGHT_REQUIRE (quote_domain.hpp) fires in every build
// configuration, including this book's Release-only CMake presets, which
// is why this test does not need a debug build to pass.
TEST(QuoteCostDeathTest, AbortsWhenCalledWithANegativeWeightPrecondition) {
    const ShipmentRequest malformed{-1.0, ServiceZone::Local, 500};

    EXPECT_DEATH(quote_cost(malformed), "weight_kg must be non-negative");
}

}  // namespace
}  // namespace freight

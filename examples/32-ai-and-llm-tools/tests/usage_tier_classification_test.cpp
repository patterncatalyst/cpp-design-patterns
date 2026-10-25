#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include "correct_usage_tier_strategy.hpp"
#include "plausible_wrong_usage_tier_strategy.hpp"
#include "usage_tier_strategy.hpp"

namespace {

// The behavior table every UsageTierStrategy is gated on before anyone
// trusts it, whether a human or an AI pairing partner wrote the
// implementation. Both lower bounds are inclusive per the pricing spec:
// a count that reaches exactly 10,000 or exactly 100,000 has crossed into
// the next tier.
void ExpectSpecCompliantClassification(const UsageTierStrategy& strategy) {
    EXPECT_EQ(strategy.Classify(0), UsageTier::Starter);
    EXPECT_EQ(strategy.Classify(9'999), UsageTier::Starter);
    EXPECT_EQ(strategy.Classify(10'000), UsageTier::Growth);
    EXPECT_EQ(strategy.Classify(50'000), UsageTier::Growth);
    EXPECT_EQ(strategy.Classify(99'999), UsageTier::Growth);
    EXPECT_EQ(strategy.Classify(100'000), UsageTier::Scale);
    EXPECT_EQ(strategy.Classify(250'000), UsageTier::Scale);
}

}  // namespace

TEST(UsageTierClassification, CorrectStrategySatisfiesTheFullBoundaryTable) {
    const CorrectUsageTierStrategy strategy;
    ExpectSpecCompliantClassification(strategy);
}

TEST(UsageTierClassification, PlausibleWrongStrategyFailsExactlyAtTheGrowthBoundary) {
    const PlausibleWrongUsageTierStrategy strategy;

    // This is the gate this chapter is about: run the identical behavior
    // table against the AI-drafted implementation rather than trusting it
    // on read. EXPECT_NONFATAL_FAILURE requires the wrapped statement to
    // produce exactly one GoogleTest failure, which is exactly what this
    // strategy produces: every case away from the boundary matches the
    // spec, and the 10,000-request case alone does not.
    EXPECT_NONFATAL_FAILURE(ExpectSpecCompliantClassification(strategy), "10'000");
}

TEST(UsageTierClassification, BothStrategiesAgreeEverywhereAwayFromTheBoundary) {
    const CorrectUsageTierStrategy correct;
    const PlausibleWrongUsageTierStrategy plausible_wrong;

    for (const long requests : {0L, 5'000L, 9'999L, 50'000L, 99'999L, 100'000L, 250'000L}) {
        EXPECT_EQ(correct.Classify(requests), plausible_wrong.Classify(requests))
            << "requests = " << requests;
    }
}

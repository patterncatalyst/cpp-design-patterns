#include "plausible_wrong_usage_tier_strategy.hpp"

UsageTier PlausibleWrongUsageTierStrategy::Classify(long monthly_requests) const {
    if (monthly_requests <= 10'000) {  // off-by-one: should be < 10'000
        return UsageTier::Starter;
    }
    if (monthly_requests < 100'000) {
        return UsageTier::Growth;
    }
    return UsageTier::Scale;
}

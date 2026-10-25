#include "correct_usage_tier_strategy.hpp"

UsageTier CorrectUsageTierStrategy::Classify(long monthly_requests) const {
    if (monthly_requests < 10'000) {
        return UsageTier::Starter;
    }
    if (monthly_requests < 100'000) {
        return UsageTier::Growth;
    }
    return UsageTier::Scale;
}

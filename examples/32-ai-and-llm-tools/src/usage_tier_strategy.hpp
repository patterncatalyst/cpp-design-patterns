#pragma once

#include "usage_tier.hpp"

// The force behind this Strategy: which billing tier a customer falls
// into varies by exactly one input, their monthly request count, and
// nothing else in the gateway needs to know which classification rule is
// active. Two implementations sit behind this interface in this example:
// one correct, one AI-plausible and subtly wrong. Both are exercised by
// the same behavior table in tests/usage_tier_classification_test.cpp.
class UsageTierStrategy {
public:
    virtual ~UsageTierStrategy() = default;

    virtual UsageTier Classify(long monthly_requests) const = 0;
};

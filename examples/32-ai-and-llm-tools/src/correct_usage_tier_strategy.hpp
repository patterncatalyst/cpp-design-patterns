#pragma once

#include "usage_tier_strategy.hpp"

// The pricing spec's actual boundaries, the forces an engineer names
// before asking an AI pairing partner to draft this Strategy:
//   Starter:  0        .. 9,999    requests/month (pay-as-you-go)
//   Growth:   10,000   .. 99,999   requests/month ($99 flat)
//   Scale:    100,000+             requests/month ($499 flat)
// Both lower bounds are INCLUSIVE: a customer whose count reaches exactly
// 10,000 or exactly 100,000 that month has crossed into the next tier.
class CorrectUsageTierStrategy final : public UsageTierStrategy {
public:
    UsageTier Classify(long monthly_requests) const override;
};

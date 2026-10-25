#pragma once

#include "usage_tier_strategy.hpp"

// AI-PLAUSIBLE, SUBTLY WRONG. This is the shape an LLM pairing partner
// handed back for the identical prompt that produced
// CorrectUsageTierStrategy: the same interface, the same three tiers, the
// same early-return structure a reviewer skimming for "does this look
// like a Strategy" would wave through. It carries exactly one known bug:
// the Starter/Growth boundary uses <= instead of <, so a customer whose
// count reaches exactly their 10,000th request of the month is still
// classified as Starter instead of Growth.
//
// tests/usage_tier_classification_test.cpp exists to catch exactly this.
// This type stays in the codebase only so that test has something real to
// run against; nothing in src/demo_main.cpp or anywhere else wires it
// into a pricing path a real bill would ever use.
class PlausibleWrongUsageTierStrategy final : public UsageTierStrategy {
public:
    UsageTier Classify(long monthly_requests) const override;
};

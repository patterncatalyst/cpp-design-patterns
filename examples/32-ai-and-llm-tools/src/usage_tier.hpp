#pragma once

// The three billing tiers a metered API gateway classifies a customer's
// monthly request volume into. Shared by every UsageTierStrategy
// implementation (src/usage_tier_strategy.hpp) and by the billing table
// (src/billing.hpp) that turns a tier into a dollar figure.
enum class UsageTier { Starter, Growth, Scale };

const char* ToString(UsageTier tier);

#pragma once

#include "usage_tier.hpp"

// Starter accounts are metered per request with no bundled allotment;
// Growth and Scale are flat monthly bundles regardless of volume within
// the tier. Used by demo_main.cpp and tests/billing_test.cpp to turn a
// UsageTierStrategy's classification into the dollar figure a customer
// actually sees, which is what makes a one-operator boundary bug in
// UsageTierStrategy a billing error, not an abstract correctness issue.
double MonthlyBillUsd(UsageTier tier, long monthly_requests);

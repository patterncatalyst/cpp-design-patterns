// examples/32-ai-and-llm-tools demo: runs the correct usage-tier
// Strategy and the AI-plausible, subtly wrong one over the same sample of
// monthly request counts, then isolates the exact boundary where they
// disagree and the dollar amount that disagreement costs a customer.

#include <iomanip>
#include <iostream>
#include <vector>

#include "billing.hpp"
#include "correct_usage_tier_strategy.hpp"
#include "plausible_wrong_usage_tier_strategy.hpp"
#include "usage_tier.hpp"
#include "usage_tier_strategy.hpp"

namespace {

void PrintClassification(const UsageTierStrategy& strategy, const char* label,
                          long monthly_requests) {
    const UsageTier tier = strategy.Classify(monthly_requests);
    std::cout << "  " << std::setw(8) << std::right << monthly_requests
              << " requests -> " << std::setw(7) << std::left << ToString(tier)
              << " ($" << std::fixed << std::setprecision(2)
              << MonthlyBillUsd(tier, monthly_requests) << "/mo)\n";
}

}  // namespace

int main() {
    const CorrectUsageTierStrategy correct;
    const PlausibleWrongUsageTierStrategy plausible_wrong;

    const std::vector<long> sample_requests{9'999, 10'000, 50'000, 99'999, 100'000};

    std::cout << "== CorrectUsageTierStrategy: lower tier boundary is inclusive ==\n";
    for (const long requests : sample_requests) {
        PrintClassification(correct, "correct", requests);
    }

    std::cout << "\n== PlausibleWrongUsageTierStrategy: AI-drafted, looks identical on read ==\n";
    for (const long requests : sample_requests) {
        PrintClassification(plausible_wrong, "wrong", requests);
    }

    const UsageTier correct_tier_at_boundary = correct.Classify(10'000);
    const UsageTier wrong_tier_at_boundary = plausible_wrong.Classify(10'000);
    const double correct_bill = MonthlyBillUsd(correct_tier_at_boundary, 10'000);
    const double wrong_bill = MonthlyBillUsd(wrong_tier_at_boundary, 10'000);

    std::cout << "\n== The boundary a read-only review missed ==\n";
    std::cout << "At exactly 10,000 requests:\n";
    std::cout << "  correct classification: " << ToString(correct_tier_at_boundary)
              << " ($" << std::fixed << std::setprecision(2) << correct_bill << "/mo)\n";
    std::cout << "  plausible-wrong classification: " << ToString(wrong_tier_at_boundary)
              << " ($" << std::fixed << std::setprecision(2) << wrong_bill << "/mo)\n";
    std::cout << "  billing error from one comparison operator: $"
              << std::fixed << std::setprecision(2) << (wrong_bill - correct_bill) << "\n";

    return 0;
}

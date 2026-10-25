#include "billing.hpp"

namespace {

constexpr double kStarterPerRequestUsd = 0.02;
constexpr double kGrowthFlatFeeUsd = 99.00;
constexpr double kScaleFlatFeeUsd = 499.00;

}  // namespace

double MonthlyBillUsd(UsageTier tier, long monthly_requests) {
    switch (tier) {
        case UsageTier::Starter:
            return static_cast<double>(monthly_requests) * kStarterPerRequestUsd;
        case UsageTier::Growth:
            return kGrowthFlatFeeUsd;
        case UsageTier::Scale:
            return kScaleFlatFeeUsd;
    }
    return 0.0;  // unreachable for a closed enum; silences -Wreturn-type.
}

#include "legacy_eta_estimator.hpp"

#include <chrono>

#include "transit_calculations.hpp"

namespace refactor29 {

EtaEstimate LegacyEtaEstimator::estimate(const std::string& carrier,
                                          const std::string& origin_zone,
                                          const std::string& destination_zone,
                                          double weight_kg,
                                          bool is_residential,
                                          double declared_value) const {
    int days = base_transit_days(carrier, origin_zone, destination_zone, declared_value);
    double surcharge = base_surcharge(weight_kg, is_residential, declared_value);

    // The hard-wired collaborator this example's second stage breaks.
    // Nothing about this line is visible from estimate()'s signature, and
    // nothing outside this function body can intercept it: a caller who
    // wants a deterministic answer has no way to get one.
    const auto now = std::chrono::system_clock::now();
    const auto today = std::chrono::year_month_day{
        std::chrono::floor<std::chrono::days>(now)};

    if (today.month() == std::chrono::November || today.month() == std::chrono::December) {
        days += 2;
        surcharge += 8.0;
    }

    const auto delivery = std::chrono::sys_days{today} + std::chrono::days{days};
    return EtaEstimate{days, surcharge, std::chrono::year_month_day{delivery}};
}

}  // namespace refactor29

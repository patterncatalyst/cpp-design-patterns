#include "seamed_eta_estimator.hpp"

#include "transit_calculations.hpp"

namespace refactor29 {

EtaEstimate SeamedEtaEstimator::estimate(const std::string& carrier,
                                          const std::string& origin_zone,
                                          const std::string& destination_zone,
                                          double weight_kg,
                                          bool is_residential,
                                          double declared_value) const {
    int days = base_transit_days(carrier, origin_zone, destination_zone, declared_value);
    double surcharge = base_surcharge(weight_kg, is_residential, declared_value);

    // The only line that changed from LegacyEtaEstimator::estimate(): a
    // direct system_clock::now() call became a call through the seam.
    const auto today = clock_.today();

    if (today.month() == std::chrono::November || today.month() == std::chrono::December) {
        days += 2;
        surcharge += 8.0;
    }

    const auto delivery = std::chrono::sys_days{today} + std::chrono::days{days};
    return EtaEstimate{days, surcharge, std::chrono::year_month_day{delivery}};
}

}  // namespace refactor29

#include "modern_eta_estimator.hpp"

#include <chrono>

#include "carrier_profile.hpp"

namespace refactor29 {
namespace {

// Extract Function: the holiday-season adjustment, previously a few lines
// buried in the middle of estimate(), now has a name and a single-purpose
// signature. The rule itself is unchanged from the legacy and seamed
// stages: November and December add two transit days and an $8 surcharge.
bool is_holiday_season(const Clock& clock) {
    const auto today = clock.today();
    return today.month() == std::chrono::November || today.month() == std::chrono::December;
}

int holiday_surcharge_days(const Clock& clock) {
    return is_holiday_season(clock) ? 2 : 0;
}

double holiday_surcharge_amount(const Clock& clock) {
    return is_holiday_season(clock) ? 8.0 : 0.0;
}

// Extract Function: weight, residential-address, and declared-value
// surcharges, previously interleaved with the carrier dispatch, now read
// as one named rule that has nothing to do with which carrier was chosen.
double weight_and_value_surcharge(const ShipmentProfile& shipment) {
    double surcharge = 0.0;
    if (shipment.weight_kg > 40.0) {
        surcharge += 15.0;
    }
    if (shipment.is_residential) {
        surcharge += 5.0;
    }
    if (shipment.declared_value > 1000.0) {
        surcharge += 12.50;
    }
    return surcharge;
}

int signature_required_days(const ShipmentProfile& shipment) {
    return shipment.declared_value > 1000.0 ? 1 : 0;
}

}  // namespace

ModernEtaEstimator::ModernEtaEstimator()
    : owned_clock_(std::make_unique<SystemClock>()), clock_(*owned_clock_) {}

ModernEtaEstimator::ModernEtaEstimator(const Clock& clock) : clock_(clock) {}

EtaEstimate ModernEtaEstimator::estimate(const ShipmentProfile& shipment) const {
    const auto profile = carrier_profile_for(shipment.carrier);
    const int days = profile->base_transit_days(shipment) + signature_required_days(shipment) +
                      holiday_surcharge_days(clock_);
    const double surcharge =
        weight_and_value_surcharge(shipment) + holiday_surcharge_amount(clock_);

    const auto today = clock_.today();
    const auto delivery = std::chrono::sys_days{today} + std::chrono::days{days};
    return EtaEstimate{days, surcharge, std::chrono::year_month_day{delivery}};
}

}  // namespace refactor29

#include "shipping_domain.hpp"

#include <array>
#include <cmath>
#include <cstddef>

namespace shipping {

Cents flat_rate_cost(Cents flat_cents) {
    return flat_cents;
}

Cents weight_based_cost(Cents base_cents, Cents per_kg_cents, double weight_kg) {
    const double variable_cost = weight_kg * static_cast<double>(per_kg_cents);
    return base_cents + static_cast<Cents>(std::ceil(variable_cost));
}

Cents tiered_zone_cost(const Shipment& shipment) {
    // Per-zone base price covers the first two kilograms; everything past
    // that threshold is billed at a per-zone per-kilogram overage rate.
    // Carriers price distance-and-weight tiers this way because handling
    // cost is dominated by distance until a parcel gets heavy.
    static constexpr std::array<Cents, 4> kZoneBaseCents = {600, 900, 1400, 2600};
    static constexpr std::array<Cents, 4> kZoneOverageCentsPerKg = {150, 220, 320, 540};
    constexpr double kBaseThresholdKg = 2.0;

    const auto zone_index = static_cast<std::size_t>(shipment.zone);
    Cents cost = kZoneBaseCents[zone_index];

    const double overage_kg = shipment.weight_kg - kBaseThresholdKg;
    if (overage_kg > 0.0) {
        const double overage_cost = overage_kg * static_cast<double>(kZoneOverageCentsPerKg[zone_index]);
        cost += static_cast<Cents>(std::ceil(overage_cost));
    }

    return cost;
}

}  // namespace shipping

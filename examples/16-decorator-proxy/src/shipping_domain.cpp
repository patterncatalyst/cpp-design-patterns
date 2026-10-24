#include "shipping_domain.hpp"

#include <array>
#include <cmath>
#include <cstddef>

namespace shipping {

Cents base_tariff_cost(const Shipment& shipment) {
    // A flat per-zone base fee covers handling; everything past that is
    // billed by weight at a per-zone per-kilogram rate. Carriers price this
    // way because handling cost is dominated by distance, not weight, until
    // a parcel gets heavy.
    static constexpr std::array<Cents, 4> kZoneBaseCents = {450, 700, 1100, 1900};
    static constexpr std::array<Cents, 4> kZonePerKgCents = {80, 120, 180, 310};

    const auto zone_index = static_cast<std::size_t>(shipment.zone);
    const double variable_cost = shipment.weight_kg * static_cast<double>(kZonePerKgCents[zone_index]);
    return kZoneBaseCents[zone_index] + static_cast<Cents>(std::ceil(variable_cost));
}

}  // namespace shipping

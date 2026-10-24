#pragma once

#include <cstdint>

namespace shipping {

// Destination zone drives the tiered-by-distance rate table. Zones are
// ordered from nearest to farthest so the tiered strategy can index
// straight into its per-zone tables with no branching per zone.
enum class DestinationZone {
    Local,
    Regional,
    National,
    International,
};

// Money in this domain is tracked as integer cents, never floating point,
// so every quote is exact and reproducible in a test assertion.
using Cents = std::int64_t;

// A parcel ready to be quoted at checkout. weight_kg and
// declared_value_cents come straight off the cart; zone is resolved from
// the shipping address before a quote is requested.
struct Shipment {
    double weight_kg;
    DestinationZone zone;
    Cents declared_value_cents;
};

// Pure rate math shared by all three Strategy expressions in this example
// (runtime polymorphism, std::function, compile-time policy), so an
// identical shipment prices identically no matter which dispatch mechanism
// quoted it. Only the dispatch differs between the three; the pricing
// rules below are the single source of truth.
Cents flat_rate_cost(Cents flat_cents);
Cents weight_based_cost(Cents base_cents, Cents per_kg_cents, double weight_kg);
Cents tiered_zone_cost(const Shipment& shipment);

}  // namespace shipping

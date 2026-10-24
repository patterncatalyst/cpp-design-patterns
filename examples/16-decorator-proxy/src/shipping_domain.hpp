#pragma once

#include <cstdint>

namespace shipping {

// Destination zone drives the per-zone base rate and per-kilogram rate in
// base_tariff_cost(). Zones are ordered nearest to farthest so the table
// lookup below can index straight into its per-zone arrays.
enum class DestinationZone {
    Local,
    Regional,
    National,
    International,
};

// Money in this domain is tracked as integer cents, never floating point,
// so every quote is exact and reproducible in a test assertion.
using Cents = std::int64_t;

// A parcel ready to be quoted or tracked. weight_kg and
// declared_value_cents come straight off the cart; zone is resolved from
// the shipping address before a quote is requested.
struct Shipment {
    double weight_kg;
    DestinationZone zone;
    Cents declared_value_cents;
};

// The one piece of shared, undecorated pricing math in this example:
// a per-zone base fee plus a per-kilogram rate. Every Decorator in
// rate_decorators.hpp wraps something that eventually calls down into
// this function; none of them duplicate it.
Cents base_tariff_cost(const Shipment& shipment);

}  // namespace shipping

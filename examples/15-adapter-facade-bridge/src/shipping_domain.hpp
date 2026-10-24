#pragma once

#include <cstdint>

namespace shipping {

// Destination zone drives both the legacy carrier's zone code and this
// book's own pricing rules. Shared by every pattern in this example:
// Adapter, Facade, and Bridge all quote or label the same Shipment type.
enum class DestinationZone {
    Local,
    Regional,
    National,
    International,
};

// Money in this domain is tracked as integer cents, never floating point,
// so a quote is exact and reproducible in a test assertion.
using Cents = std::int64_t;

// A parcel ready to be quoted, labeled, and tracked at checkout.
struct Shipment {
    double weight_kg;
    DestinationZone zone;
    Cents declared_value_cents;
};

}  // namespace shipping

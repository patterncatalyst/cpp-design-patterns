#pragma once

#include <cstdint>

namespace shipping {

// Destination zone and the declared value drive InsuredQuoter's surcharge
// in this example; the shape matches every other chapter's Shipment so
// this book's recurring checkout domain stays recognizable here too.
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

}  // namespace shipping

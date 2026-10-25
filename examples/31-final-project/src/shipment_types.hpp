#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace finalproject31 {

// Money is always an integer count of cents in this domain, never a
// double, so a quote is exact and a test assertion compares it without an
// epsilon anywhere in sight.
using Cents = std::int64_t;

// The shipment lifecycle this example's State component enforces. Every
// shipment starts Created and only ever moves forward.
enum class ShipmentStatus {
    Created,
    PickedUp,
    InTransit,
    Delivered,
};

// The three legal events a dispatcher can ask a shipment's lifecycle to
// honor. There is no event for every (status, status) pair on purpose:
// illegal combinations are rejected, not silently accepted.
enum class ShipmentEvent {
    PickUp,
    Dispatch,
    Deliver,
};

// Thrown when an event does not apply to a shipment's current status. The
// message names the offending status and event so a caller sees exactly
// what it asked for and why that was rejected.
class IllegalTransitionError : public std::logic_error {
public:
    IllegalTransitionError(ShipmentStatus current, ShipmentEvent event);
};

std::string to_string(ShipmentStatus status);
std::string to_string(ShipmentEvent event);

}  // namespace finalproject31

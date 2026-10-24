#pragma once

#include <stdexcept>
#include <string_view>

namespace shipment {

// The shared vocabulary every expression in this example builds on: the
// statuses a shipment can be in and the events that move it between them.
// Each of the four expressions below (GoF State, enum+table, std::variant,
// stochastic) encodes the legal moves on its own, in its own idiom, so the
// test suite can prove all four agree rather than share one engine.
enum class ShipmentStatus {
    Created,
    Paid,
    InTransit,
    Delivered,
    Cancelled,
    Lost,
};

// Advance is not a caller-chosen event; it names the stochastic machine's
// own weighted step for error messages when that step is attempted from a
// status that is not InTransit.
enum class ShipmentEvent {
    Pay,
    Ship,
    Deliver,
    Cancel,
    Advance,
};

std::string_view to_string(ShipmentStatus status);
std::string_view to_string(ShipmentEvent event);

// Thrown by every expression in this example when an event is not legal
// from the shipment's current status, e.g. shipping a shipment that has
// not been paid for yet, or advancing one that is not InTransit.
class IllegalTransition : public std::logic_error {
public:
    IllegalTransition(ShipmentStatus from, ShipmentEvent event);
};

}  // namespace shipment

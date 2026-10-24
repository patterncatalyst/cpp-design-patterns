#pragma once

#include <variant>

#include "shipment_domain.hpp"

namespace shipment {

// Expression 2b: the same finite state machine as a std::variant of state
// types, dispatched with std::visit. Each tag type below corresponds to
// exactly one ShipmentStatus; apply_event() pattern-matches on the active
// alternative and returns a new variant holding the next state. There is
// no mutable status field anywhere, and no base class: the "type" of the
// variant's active alternative is the state.
struct CreatedTag {};
struct PaidTag {};
struct InTransitTag {};
struct DeliveredTag {};
struct CancelledTag {};

using ShipmentVariant = std::variant<CreatedTag, PaidTag, InTransitTag, DeliveredTag, CancelledTag>;

ShipmentStatus status_of(const ShipmentVariant& state);

// Throws IllegalTransition if event is not legal from the active
// alternative, the same contract every other expression in this example
// uses for a rejected event.
ShipmentVariant apply_event(const ShipmentVariant& state, ShipmentEvent event);

}  // namespace shipment

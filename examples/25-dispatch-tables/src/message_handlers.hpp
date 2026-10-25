#pragma once

// The handler functions every dispatch form in this example routes to.
// There is exactly one implementation of "what happens when a shipment is
// delivered," shared by the jump table, the constexpr array table, and the
// adapters the dispatch map wraps them in. A fourth, "unrecognized,"
// handler is what every form falls back to when the message it was handed
// does not resolve to a known handler.

#include "carrier_message.hpp"
#include "tracking_ledger.hpp"

namespace dispatch25 {

using MessageHandler = void (*)(const CarrierMessage&, TrackingSink&);

void handle_created(const CarrierMessage& message, TrackingSink& sink);
void handle_picked_up(const CarrierMessage& message, TrackingSink& sink);
void handle_in_transit(const CarrierMessage& message, TrackingSink& sink);
void handle_out_for_delivery(const CarrierMessage& message, TrackingSink& sink);
void handle_delivered(const CarrierMessage& message, TrackingSink& sink);
void handle_delivery_exception(const CarrierMessage& message, TrackingSink& sink);

// Falls back for a CarrierMessage whose type did not resolve to a table
// slot (an out-of-range index from a garbled wire value, for instance).
void handle_unrecognized_type(const CarrierMessage& message, TrackingSink& sink);

}  // namespace dispatch25

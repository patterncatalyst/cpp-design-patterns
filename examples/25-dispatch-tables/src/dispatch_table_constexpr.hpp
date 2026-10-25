#pragma once

// Form three: the same closed set as the jump table, built at compile time
// instead of positionally. make_dispatch_table() assigns each handler to
// an explicit index derived from the enumerator itself, not from where the
// initializer happens to sit in a list, and the static_assert right below
// it proves every slot got filled before this translation unit finishes
// compiling. Insert a new CarrierMessageType and forget its line inside
// make_dispatch_table() and the build fails here, at the static_assert,
// instead of shipping a nullptr the jump table would happily call.

#include <algorithm>
#include <array>

#include "message_handlers.hpp"

namespace dispatch25 {

constexpr std::array<MessageHandler, kCarrierMessageTypeCount> make_dispatch_table() {
    std::array<MessageHandler, kCarrierMessageTypeCount> table{};
    table[message_type_index(CarrierMessageType::Created)] = &handle_created;
    table[message_type_index(CarrierMessageType::PickedUp)] = &handle_picked_up;
    table[message_type_index(CarrierMessageType::InTransit)] = &handle_in_transit;
    table[message_type_index(CarrierMessageType::OutForDelivery)] = &handle_out_for_delivery;
    table[message_type_index(CarrierMessageType::Delivered)] = &handle_delivered;
    table[message_type_index(CarrierMessageType::DeliveryException)] = &handle_delivery_exception;
    return table;
}

inline constexpr auto kDispatchTable = make_dispatch_table();

static_assert(std::ranges::none_of(kDispatchTable, [](MessageHandler handler) { return handler == nullptr; }),
              "every CarrierMessageType must have a handler entry in kDispatchTable");

void dispatch_via_constexpr_table(const CarrierMessage& message, TrackingSink& sink);

}  // namespace dispatch25

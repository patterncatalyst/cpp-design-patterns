#include "jump_table.hpp"

namespace dispatch25 {

// Positional: slot i must be the handler for the CarrierMessageType whose
// underlying value is i. Insert a new enumerator in the middle of the
// CarrierMessageType declaration without inserting its row here in the
// same position, and every row below the insertion point now answers for
// the wrong message type. No compiler diagnostic catches that; only a
// test, or a production incident, does.
MessageHandler g_jump_table[kCarrierMessageTypeCount] = {
    &handle_created,
    &handle_picked_up,
    &handle_in_transit,
    &handle_out_for_delivery,
    &handle_delivered,
    &handle_delivery_exception,
};

void dispatch_via_jump_table(const CarrierMessage& message, TrackingSink& sink) {
    const std::size_t index = message_type_index(message.type);
    if (index >= kCarrierMessageTypeCount) {
        handle_unrecognized_type(message, sink);
        return;
    }
    g_jump_table[index](message, sink);
}

}  // namespace dispatch25

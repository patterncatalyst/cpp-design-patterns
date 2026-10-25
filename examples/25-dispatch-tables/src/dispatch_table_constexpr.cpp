#include "dispatch_table_constexpr.hpp"

namespace dispatch25 {

void dispatch_via_constexpr_table(const CarrierMessage& message, TrackingSink& sink) {
    const std::size_t index = message_type_index(message.type);
    if (index >= kCarrierMessageTypeCount) {
        handle_unrecognized_type(message, sink);
        return;
    }
    kDispatchTable[index](message, sink);
}

}  // namespace dispatch25

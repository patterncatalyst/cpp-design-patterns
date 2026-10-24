#include "table_fsm.hpp"

namespace shipment {

std::size_t ShipmentTransitionKeyHash::operator()(const ShipmentTransitionKey& key) const noexcept {
    const auto from_bits = static_cast<std::size_t>(key.from);
    const auto event_bits = static_cast<std::size_t>(key.event);
    return (from_bits << 4) ^ event_bits;
}

const ShipmentTransitionTable& shipment_transition_table() {
    static const ShipmentTransitionTable table{
        {{ShipmentStatus::Created, ShipmentEvent::Pay}, ShipmentStatus::Paid},
        {{ShipmentStatus::Created, ShipmentEvent::Cancel}, ShipmentStatus::Cancelled},
        {{ShipmentStatus::Paid, ShipmentEvent::Ship}, ShipmentStatus::InTransit},
        {{ShipmentStatus::Paid, ShipmentEvent::Cancel}, ShipmentStatus::Cancelled},
        {{ShipmentStatus::InTransit, ShipmentEvent::Deliver}, ShipmentStatus::Delivered},
    };
    return table;
}

TableShipmentMachine::TableShipmentMachine() : status_(ShipmentStatus::Created) {}

void TableShipmentMachine::apply(ShipmentEvent event) {
    const auto& table = shipment_transition_table();
    const auto it = table.find(ShipmentTransitionKey{status_, event});
    if (it == table.end()) {
        throw IllegalTransition(status_, event);
    }
    status_ = it->second;
}

#if __cplusplus >= 202302L
std::expected<ShipmentStatus, std::string> TableShipmentMachine::try_apply(ShipmentEvent event) {
    const auto& table = shipment_transition_table();
    const auto it = table.find(ShipmentTransitionKey{status_, event});
    if (it == table.end()) {
        return std::unexpected<std::string>(
            std::string("cannot apply event '") + std::string(to_string(event)) + "' to a shipment in status '" +
            std::string(to_string(status_)) + "'");
    }
    status_ = it->second;
    return status_;
}
#endif

}  // namespace shipment

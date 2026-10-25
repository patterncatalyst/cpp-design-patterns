#include "advance_lifecycle_command.hpp"

#include <stdexcept>

namespace finalproject31 {

AdvanceLifecycleCommand::AdvanceLifecycleCommand(Shipment& shipment, ShipmentEvent event)
    : shipment_(shipment), event_(event) {}

void AdvanceLifecycleCommand::execute() {
    previous_status_ = shipment_.status();
    shipment_.advance(event_);
    executed_ = true;
}

void AdvanceLifecycleCommand::undo() {
    if (!executed_) {
        throw std::logic_error("AdvanceLifecycleCommand::undo() called before execute()");
    }
    shipment_.restore_status_for_undo(previous_status_);
}

std::string AdvanceLifecycleCommand::description() const {
    return "advance shipment '" + shipment_.id() + "' via event '" + to_string(event_) + "'";
}

}  // namespace finalproject31

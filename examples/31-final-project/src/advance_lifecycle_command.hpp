#pragma once

#include "shipment.hpp"
#include "shipment_command.hpp"
#include "shipment_types.hpp"

namespace finalproject31 {

// Where Command meets State: execute() asks the shipment's State machine
// to honor one event. undo() does not ask the state machine to run a
// reverse event, real carriers do not un-pick-up a package, it restores
// the status snapshot taken before execute() ran. That snapshot is this
// command's inverse, the same technique Command uses for any action with
// no clean arithmetic opposite.
class AdvanceLifecycleCommand : public ShipmentCommand {
public:
    AdvanceLifecycleCommand(Shipment& shipment, ShipmentEvent event);

    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    Shipment& shipment_;
    ShipmentEvent event_;
    ShipmentStatus previous_status_ = ShipmentStatus::Created;
    bool executed_ = false;
};

}  // namespace finalproject31

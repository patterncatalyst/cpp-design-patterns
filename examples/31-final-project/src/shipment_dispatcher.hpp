#pragma once

#include <memory>
#include <optional>
#include <string>

#include "carrier_factory.hpp"
#include "command_history.hpp"
#include "lifecycle_observer.hpp"
#include "shipment.hpp"
#include "shipment_command.hpp"
#include "shipment_types.hpp"

namespace finalproject31 {

// ShipmentDispatcher is the composition root: the one place in this
// example that knows about all five patterns at once. open_shipment()
// calls CarrierFactory to build a CarrierProfile, which is where Factory
// hands back the RateStrategy every rate-sensitive command in this system
// will use. make_advance_command() and make_adjust_weight_command() wire
// a Command to this dispatcher's Shipment, closing over that same
// RateStrategy where a command needs to invoke it. apply() and undo_last()
// run commands through a CommandHistory. subscribe() registers a
// LifecycleObserver directly on the Shipment, so every legal transition
// and every undo of one fans out to whoever is listening. No caller of
// ShipmentDispatcher ever names a concrete carrier, a concrete RateStrategy,
// or a concrete Command.
class ShipmentDispatcher {
public:
    explicit ShipmentDispatcher(CarrierFactory factory = CarrierFactory{});

    // Builds the carrier's components via the factory, computes the
    // shipment's opening quote from the carrier's own RateStrategy, and
    // constructs the Shipment this dispatcher tracks. Throws
    // std::invalid_argument if the request names an unrecognized carrier.
    Shipment& open_shipment(std::string shipment_id, const CarrierRequest& request, double weight_kg);

    Shipment& shipment();
    const Shipment& shipment() const;
    const CarrierProfile& carrier_profile() const;

    void subscribe(LifecycleObserver& observer);

    std::unique_ptr<ShipmentCommand> make_advance_command(ShipmentEvent event);
    std::unique_ptr<ShipmentCommand> make_adjust_weight_command(double new_weight_kg);

    void apply(std::unique_ptr<ShipmentCommand> command);
    bool undo_last();

private:
    CarrierFactory factory_;
    std::optional<CarrierProfile> carrier_profile_;
    std::optional<Shipment> shipment_;
    CommandHistory history_;
};

}  // namespace finalproject31

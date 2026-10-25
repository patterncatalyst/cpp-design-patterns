#include "shipment_dispatcher.hpp"

#include <stdexcept>
#include <utility>

#include "adjust_weight_command.hpp"
#include "advance_lifecycle_command.hpp"

namespace finalproject31 {

ShipmentDispatcher::ShipmentDispatcher(CarrierFactory factory) : factory_(std::move(factory)) {}

Shipment& ShipmentDispatcher::open_shipment(std::string shipment_id, const CarrierRequest& request,
                                             double weight_kg) {
    // Factory: the dispatcher never names FlatRateStrategy or
    // WeightTieredRateStrategy directly. It asks for whatever the
    // request's carrier_code calls for and gets back a matching, internally
    // consistent bundle.
    carrier_profile_ = factory_.create(request);

    shipment_.emplace(std::move(shipment_id), carrier_profile_->carrier_name, request.destination_zone, weight_kg,
                       Cents{0});
    // Strategy: the opening quote comes from the RateStrategy the factory
    // just built, computed against the shipment it now describes.
    shipment_->set_quoted_rate_cents(carrier_profile_->rate_strategy->quote(*shipment_));
    return *shipment_;
}

Shipment& ShipmentDispatcher::shipment() {
    if (!shipment_) {
        throw std::logic_error("ShipmentDispatcher::shipment() called before open_shipment()");
    }
    return *shipment_;
}

const Shipment& ShipmentDispatcher::shipment() const {
    if (!shipment_) {
        throw std::logic_error("ShipmentDispatcher::shipment() called before open_shipment()");
    }
    return *shipment_;
}

const CarrierProfile& ShipmentDispatcher::carrier_profile() const {
    if (!carrier_profile_) {
        throw std::logic_error("ShipmentDispatcher::carrier_profile() called before open_shipment()");
    }
    return *carrier_profile_;
}

void ShipmentDispatcher::subscribe(LifecycleObserver& observer) { shipment().subscribe(observer); }

std::unique_ptr<ShipmentCommand> ShipmentDispatcher::make_advance_command(ShipmentEvent event) {
    return std::make_unique<AdvanceLifecycleCommand>(shipment(), event);
}

std::unique_ptr<ShipmentCommand> ShipmentDispatcher::make_adjust_weight_command(double new_weight_kg) {
    // Command meets Strategy right here: the command this factory method
    // builds closes over the same RateStrategy open_shipment() got from
    // CarrierFactory, so a weight correction and the shipment's opening
    // quote are always priced by the identical rule.
    return std::make_unique<AdjustWeightCommand>(shipment(), *carrier_profile().rate_strategy, new_weight_kg);
}

void ShipmentDispatcher::apply(std::unique_ptr<ShipmentCommand> command) { history_.execute(std::move(command)); }

bool ShipmentDispatcher::undo_last() { return history_.undo_last(); }

}  // namespace finalproject31

#include "dispatch_mediator.hpp"

#include <cassert>

namespace shipping {

void DispatchColleague::raise(DispatchEvent event, const std::string& order_id) {
    assert(mediator_ != nullptr && "colleague raised an event before a DispatchCoordinator bound it");
    mediator_->notify(*this, event, order_id);
}

void WarehouseStation::mark_order_ready(const std::string& order_id) {
    raise(DispatchEvent::OrderReadyForPickup, order_id);
}

void CarrierStation::confirm_pickup(const std::string& order_id) {
    picked_up_orders_.push_back(order_id);
    raise(DispatchEvent::PickedUpByCarrier, order_id);
}

void CarrierStation::mark_out_for_delivery(const std::string& order_id) {
    raise(DispatchEvent::OutForDelivery, order_id);
}

void CarrierStation::report_exception(const std::string& order_id, const std::string& reason) {
    exception_reasons_.push_back(reason);
    raise(DispatchEvent::DeliveryException, order_id);
}

void CustomerNotificationStation::send(const std::string& order_id, const std::string& message) {
    sent_messages_.push_back(order_id + ": " + message);
}

DispatchCoordinator::DispatchCoordinator(WarehouseStation& warehouse, CarrierStation& carrier,
                                          CustomerNotificationStation& notifier)
    : warehouse_(&warehouse), carrier_(&carrier), notifier_(&notifier) {
    warehouse.bind(*this);
    carrier.bind(*this);
    notifier.bind(*this);
}

void DispatchCoordinator::notify(DispatchColleague& sender, DispatchEvent event, const std::string& order_id) {
    switch (event) {
        case DispatchEvent::OrderReadyForPickup:
            assert(&sender == warehouse_);
            carrier_->confirm_pickup(order_id);
            break;
        case DispatchEvent::PickedUpByCarrier:
            assert(&sender == carrier_);
            notifier_->send(order_id, "Your package has been picked up and is on its way.");
            break;
        case DispatchEvent::OutForDelivery:
            assert(&sender == carrier_);
            notifier_->send(order_id, "Your package is out for delivery today.");
            break;
        case DispatchEvent::DeliveryException:
            assert(&sender == carrier_);
            notifier_->send(order_id, "There is a delivery exception on your package; our team is investigating.");
            break;
    }
}

}  // namespace shipping

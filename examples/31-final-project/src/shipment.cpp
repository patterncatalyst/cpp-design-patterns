#include "shipment.hpp"

#include <algorithm>

namespace finalproject31 {

Shipment::Shipment(std::string id, std::string carrier_name, std::string destination_zone, double weight_kg,
                     Cents quoted_rate_cents)
    : id_(std::move(id)),
      carrier_name_(std::move(carrier_name)),
      destination_zone_(std::move(destination_zone)),
      weight_kg_(weight_kg),
      quoted_rate_cents_(quoted_rate_cents) {}

void Shipment::advance(ShipmentEvent event) {
    // The whole transition table, read top to bottom as the four legal
    // statuses in order: each event moves the shipment exactly one step
    // forward, and no event applies to more than one status.
    ShipmentStatus next;
    switch (status_) {
        case ShipmentStatus::Created:
            if (event != ShipmentEvent::PickUp) {
                throw IllegalTransitionError(status_, event);
            }
            next = ShipmentStatus::PickedUp;
            break;
        case ShipmentStatus::PickedUp:
            if (event != ShipmentEvent::Dispatch) {
                throw IllegalTransitionError(status_, event);
            }
            next = ShipmentStatus::InTransit;
            break;
        case ShipmentStatus::InTransit:
            if (event != ShipmentEvent::Deliver) {
                throw IllegalTransitionError(status_, event);
            }
            next = ShipmentStatus::Delivered;
            break;
        case ShipmentStatus::Delivered:
        default:
            throw IllegalTransitionError(status_, event);
    }

    const ShipmentStatus previous = status_;
    status_ = next;
    notify(previous, status_);
}

void Shipment::restore_status_for_undo(ShipmentStatus status) {
    const ShipmentStatus previous = status_;
    status_ = status;
    notify(previous, status_);
}

void Shipment::subscribe(LifecycleObserver& observer) { observers_.push_back(&observer); }

void Shipment::unsubscribe(LifecycleObserver& observer) {
    observers_.erase(std::remove(observers_.begin(), observers_.end(), &observer), observers_.end());
}

void Shipment::notify(ShipmentStatus from, ShipmentStatus to) {
    for (LifecycleObserver* observer : observers_) {
        observer->on_transitioned(*this, from, to);
    }
}

}  // namespace finalproject31

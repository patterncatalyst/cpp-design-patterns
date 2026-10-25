#include "shipment_types.hpp"

namespace finalproject31 {

namespace {

std::string make_message(ShipmentStatus current, ShipmentEvent event) {
    return "illegal transition: event '" + to_string(event) + "' does not apply to status '" +
           to_string(current) + "'";
}

}  // namespace

IllegalTransitionError::IllegalTransitionError(ShipmentStatus current, ShipmentEvent event)
    : std::logic_error(make_message(current, event)) {}

std::string to_string(ShipmentStatus status) {
    switch (status) {
        case ShipmentStatus::Created:
            return "Created";
        case ShipmentStatus::PickedUp:
            return "PickedUp";
        case ShipmentStatus::InTransit:
            return "InTransit";
        case ShipmentStatus::Delivered:
            return "Delivered";
    }
    return "Unknown";
}

std::string to_string(ShipmentEvent event) {
    switch (event) {
        case ShipmentEvent::PickUp:
            return "PickUp";
        case ShipmentEvent::Dispatch:
            return "Dispatch";
        case ShipmentEvent::Deliver:
            return "Deliver";
    }
    return "Unknown";
}

}  // namespace finalproject31

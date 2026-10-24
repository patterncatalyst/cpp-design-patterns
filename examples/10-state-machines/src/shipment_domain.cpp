#include "shipment_domain.hpp"

#include <string>

namespace shipment {

std::string_view to_string(ShipmentStatus status) {
    switch (status) {
        case ShipmentStatus::Created: return "Created";
        case ShipmentStatus::Paid: return "Paid";
        case ShipmentStatus::InTransit: return "InTransit";
        case ShipmentStatus::Delivered: return "Delivered";
        case ShipmentStatus::Cancelled: return "Cancelled";
        case ShipmentStatus::Lost: return "Lost";
    }
    return "Unknown";
}

std::string_view to_string(ShipmentEvent event) {
    switch (event) {
        case ShipmentEvent::Pay: return "Pay";
        case ShipmentEvent::Ship: return "Ship";
        case ShipmentEvent::Deliver: return "Deliver";
        case ShipmentEvent::Cancel: return "Cancel";
        case ShipmentEvent::Advance: return "Advance";
    }
    return "Unknown";
}

namespace {

std::string illegal_transition_message(ShipmentStatus from, ShipmentEvent event) {
    return std::string("cannot apply event '") + std::string(to_string(event)) +
           "' to a shipment in status '" + std::string(to_string(from)) + "'";
}

}  // namespace

IllegalTransition::IllegalTransition(ShipmentStatus from, ShipmentEvent event)
    : std::logic_error(illegal_transition_message(from, event)) {}

}  // namespace shipment

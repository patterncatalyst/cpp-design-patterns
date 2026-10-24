#include "exception_handler.hpp"

namespace palletizing {

void NullShipmentExceptionHandler::on_overweight(const std::string& /*group_label*/,
                                                   int /*grams_over_limit*/) const {
    // Intentionally empty. The Null Object's entire job is to be safely
    // callable and change nothing.
}

const ShipmentExceptionHandler& null_shipment_exception_handler() {
    static const NullShipmentExceptionHandler instance;
    return instance;
}

void LoggingShipmentExceptionHandler::on_overweight(const std::string& group_label, int grams_over_limit) const {
    events_.push_back(group_label + " is " + std::to_string(grams_over_limit) + "g over its weight limit");
}

}  // namespace palletizing

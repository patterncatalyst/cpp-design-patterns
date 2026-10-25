#include "shipment_pricing_service.hpp"

#include <stdexcept>

namespace antipattern26 {

PricingResult ShipmentPricingService::price(double base_cost) const {
    if (base_cost <= 0.0) {
        throw std::invalid_argument("base_cost must be positive");
    }

    const double total = base_cost * settings_.surcharge_multiplier + settings_.handling_fee;
    const bool surcharge_applied = settings_.surcharge_multiplier != 1.0;
    return PricingResult{total, surcharge_applied};
}

}  // namespace antipattern26

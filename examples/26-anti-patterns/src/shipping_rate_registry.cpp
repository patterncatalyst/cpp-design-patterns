#include "shipping_rate_registry.hpp"

namespace antipattern26 {

ShippingRateRegistry& ShippingRateRegistry::instance() {
    static ShippingRateRegistry registry;
    return registry;
}

void ShippingRateRegistry::set_surcharge_multiplier(double multiplier) {
    std::lock_guard<std::mutex> lock(mutex_);
    surcharge_multiplier_ = multiplier;
}

double ShippingRateRegistry::surcharge_multiplier() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return surcharge_multiplier_;
}

}  // namespace antipattern26

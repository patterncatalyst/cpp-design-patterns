#include "rate_calculator.hpp"

#include "shipping_rate_registry.hpp"

namespace antipattern26 {

double StandardRateCalculator::calculate(double base_cost) const {
    return base_cost * ShippingRateRegistry::instance().surcharge_multiplier();
}

}  // namespace antipattern26

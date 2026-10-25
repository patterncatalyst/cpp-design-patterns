#include "shipment_operations_manager.hpp"

#include <stdexcept>

#include "rate_calculator.hpp"
#include "rate_calculator_factory.hpp"
#include "shipping_rate_registry.hpp"

namespace antipattern26 {

void ShipmentOperationsManager::validate_base_cost(double base_cost) const {
    if (base_cost <= 0.0) {
        throw std::invalid_argument("base_cost must be positive");
    }
}

void ShipmentOperationsManager::log_event(std::string event) {
    log_.push_back(std::move(event));
}

PricedShipmentReceipt ShipmentOperationsManager::price_shipment(const std::string& order_id,
                                                                 double base_cost) {
    validate_base_cost(base_cost);

    auto calculator = RateCalculatorFactoryProvider::instance().factory().create_calculator();
    const double total = calculator->calculate(base_cost);

    // Bookkeeping for was_last_calculation_surcharge_applied(), tracked on
    // the manager rather than carried on the receipt. This is the leaky
    // abstraction: the fact belongs on PricedShipmentReceipt, not here.
    last_surcharge_multiplier_ = ShippingRateRegistry::instance().surcharge_multiplier();

    log_event("priced " + order_id + " at " + std::to_string(total));

    return PricedShipmentReceipt{order_id, total};
}

bool ShipmentOperationsManager::was_last_calculation_surcharge_applied() const {
    return last_surcharge_multiplier_ != 1.0;
}

const std::vector<std::string>& ShipmentOperationsManager::operation_log() const {
    return log_;
}

std::string ShipmentOperationsManager::format_receipt(const PricedShipmentReceipt& receipt) const {
    return "Order " + receipt.order_id + ": $" + std::to_string(receipt.total_cost);
}

}  // namespace antipattern26

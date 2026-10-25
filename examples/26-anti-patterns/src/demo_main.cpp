#include <iomanip>
#include <iostream>

#include "rate_calculator_factory.hpp"
#include "shipment_operations_manager.hpp"
#include "shipment_pricing_service.hpp"
#include "shipping_rate_registry.hpp"

namespace {

void print_money(const char* label, double amount) {
    std::cout << label << ": $" << std::fixed << std::setprecision(2) << amount << '\n';
}

}  // namespace

int main() {
    using namespace antipattern26;

    std::cout << "== Over-patterned (BEFORE): singleton + factory-of-factories ==\n";
    ShipmentOperationsManager manager;

    ShippingRateRegistry::instance().set_surcharge_multiplier(1.10);
    auto region_a_calculator = RateCalculatorFactoryProvider::instance().factory().create_calculator();
    print_money("Region A quote (surcharge 1.10)", region_a_calculator->calculate(100.0));

    const PricedShipmentReceipt receipt_a = manager.price_shipment("ORD-3001", 100.0);
    print_money("ORD-3001 priced via the manager", receipt_a.total_cost);

    // A second region configures the one global slot. Nothing about
    // region_a_calculator changed; its next answer still changes anyway.
    ShippingRateRegistry::instance().set_surcharge_multiplier(1.30);
    const PricedShipmentReceipt receipt_b = manager.price_shipment("ORD-3002", 100.0);
    print_money("ORD-3002 priced via the manager", receipt_b.total_cost);

    print_money("Region A quote, re-read with no code touching it", region_a_calculator->calculate(100.0));
    std::cout << "(the surcharge moved out from under region A; nothing about\n"
                 " region_a_calculator was ever reassigned)\n";

    std::cout << "was_last_calculation_surcharge_applied() after ORD-3002: "
              << (manager.was_last_calculation_surcharge_applied() ? "true" : "false")
              << " -- answers for ORD-3002, not for the ORD-3001 receipt still held above\n";

    std::cout << "\n== Simplified (AFTER): plain dependency injection of RateSettings ==\n";
    const ShipmentPricingService domestic(RateSettings{1.0, 2.50});
    const ShipmentPricingService international(RateSettings{1.35, 9.00});

    const PricingResult domestic_result = domestic.price(100.0);
    const PricingResult international_result = international.price(100.0);

    print_money("Domestic total (surcharge 1.00)", domestic_result.total_cost);
    std::cout << "Domestic surcharge applied: " << (domestic_result.surcharge_applied ? "true" : "false") << '\n';

    print_money("International total (surcharge 1.35)", international_result.total_cost);
    std::cout << "International surcharge applied: "
              << (international_result.surcharge_applied ? "true" : "false") << '\n';

    const PricingResult domestic_again = domestic.price(100.0);
    print_money("Domestic total, re-read after pricing international", domestic_again.total_cost);
    std::cout << "(unchanged: domestic and international each hold their own\n"
                 " RateSettings value, so pricing one never moves the other)\n";

    return 0;
}

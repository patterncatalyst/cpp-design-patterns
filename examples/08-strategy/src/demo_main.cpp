// Shipping-rate estimation at checkout, priced three idiomatic C++ ways:
// runtime polymorphism, a std::function callable, and a compile-time
// policy. All three price the exact same Shipment identically, because
// they all call down into the same pure rate math in shipping_domain.cpp.
//
// Build and run via scripts/build.sh from the repo root, or standalone
// from this directory: see README.md "Build, run, observe".

#include <iostream>
#include <memory>

#include "function_strategy.hpp"
#include "policy_strategy.hpp"
#include "runtime_strategy.hpp"
#include "shipping_domain.hpp"

namespace {

void print_quote(const char* label, shipping::Cents cents) {
    const auto dollars = cents / 100;
    const auto remainder = cents % 100;
    std::cout << label << ": $" << dollars << "." << (remainder < 10 ? "0" : "") << remainder << '\n';
}

}  // namespace

int main() {
    using namespace shipping;

    const Shipment cart_item{/*weight_kg=*/3.5, DestinationZone::Regional, /*declared_value_cents=*/12000};

    std::cout << "== Strategy 1: runtime polymorphism (unique_ptr injection) ==\n";
    RateEstimator estimator(std::make_unique<WeightBasedCarrier>(/*base_cents=*/500, /*per_kg_cents=*/150));
    print_quote("Weight-based carrier", estimator.estimate(cart_item));
    estimator.use_strategy(std::make_unique<TieredZoneCarrier>());
    print_quote("Tiered-zone carrier (swapped at runtime)", estimator.estimate(cart_item));

    std::cout << "\n== Strategy 2: std::function value-semantic callable ==\n";
    QuoteBook quote_book(flat_rate(999));
    print_quote("Flat-rate carrier", quote_book.quote(cart_item));
    quote_book.use(with_insurance_surcharge(tiered_zone(), /*cents_per_declared_dollar=*/1.5));
    print_quote("Tiered-zone carrier + insurance (composed)", quote_book.quote(cart_item));

    std::cout << "\n== Strategy 3: compile-time policy (concept-constrained template) ==\n";
    StaticRateCalculator<WeightBasedPolicy> weight_calculator{WeightBasedPolicy{500, 150}};
    print_quote("Weight-based policy", weight_calculator.estimate(cart_item));
    StaticRateCalculator<TieredZonePolicy> tiered_calculator{TieredZonePolicy{}};
    print_quote("Tiered-zone policy", tiered_calculator.estimate(cart_item));

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: typed validation via std::expected ==\n";
    const Shipment malformed_shipment{/*weight_kg=*/-2.0, DestinationZone::Local, /*declared_value_cents=*/0};
    if (const auto result = estimator.try_estimate(malformed_shipment); !result.has_value()) {
        std::cout << "Rejected malformed shipment: " << result.error() << '\n';
    }
#endif

    return 0;
}

// CRTP and policy-based design, both resolved entirely at compile time:
// no vtable, no type-erased callable, no indirection at any call site in
// this file. Build and run via scripts/build.sh from the repo root, or
// standalone from this directory: see README.md "Build, run, observe".

#include <iostream>

#include "comparable_mixin.hpp"
#include "quote_types.hpp"
#include "rate_model.hpp"
#include "rate_model_deducing_this.hpp"
#include "shipment_types.hpp"
#include "shipping_calculator.hpp"

namespace {

void print_quote(const char* label, crtp21::Cents cents) {
    const auto dollars = cents / 100;
    const auto remainder = cents % 100;
    std::cout << label << ": $" << dollars << "." << (remainder < 10 ? "0" : "") << remainder << '\n';
}

}  // namespace

int main() {
    using namespace crtp21;

    const Shipment cart_item{/*weight_kg=*/3.5, /*declared_value_cents=*/12345};

    std::cout << "== CRTP: RateModel<Derived> (static polymorphism) ==\n";
    const FlatRateModel flat_model(999);
    const WeightedRateModel weighted_model(/*base_cents=*/500, /*per_kg_cents=*/150);
    print_quote("Flat rate model", flat_model.quote(cart_item));
    print_quote("Weighted rate model", weighted_model.quote(cart_item));

    std::cout << "\n== CRTP mixin: Comparable<Derived> across unrelated types ==\n";
    const ShipmentQuote cheaper_quote(flat_model.quote(cart_item));
    const ShipmentQuote pricier_quote(weighted_model.quote(cart_item));
    std::cout << "Flat quote < weighted quote? " << std::boolalpha << (cheaper_quote < pricier_quote) << '\n';
    const CarrierRating carrier_a(4.6);
    const CarrierRating carrier_b(4.2);
    std::cout << "Carrier A rated higher than carrier B? " << (carrier_a > carrier_b) << '\n';

    std::cout << "\n== Policy-based design: ShippingCalculator<Rounding, Surcharge> ==\n";
    const ShippingCalculator<NoRoundingPolicy, NoSurchargePolicy> base_only(1000);
    print_quote("Base rate, no rounding, no surcharge", base_only.quote(cart_item));

    const ShippingCalculator<RoundUpToNickelPolicy, FlatSurchargePolicy> flat_plus_nickel(
        1000, RoundUpToNickelPolicy{}, FlatSurchargePolicy{153});
    print_quote("Base rate + flat surcharge, rounded to the nickel", flat_plus_nickel.quote(cart_item));

    const ShippingCalculator<RoundUpToNickelPolicy, DeclaredValueSurchargePolicy> declared_value_plus_nickel(
        1000, RoundUpToNickelPolicy{}, DeclaredValueSurchargePolicy{});
    print_quote("Base rate + 1% declared-value surcharge, rounded to the nickel",
                declared_value_plus_nickel.quote(cart_item));

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: deducing this instead of CRTP ==\n";
    const DeducedFlatRateModel deduced_flat_model(999);
    print_quote("Deduced flat rate model (no template argument on the base)", deduced_flat_model.quote(cart_item));
#endif

    return 0;
}

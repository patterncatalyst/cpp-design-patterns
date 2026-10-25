#pragma once

namespace antipattern26 {

// RateSettings is the entire configuration surface the over-patterned
// design spent five types and two singletons expressing: a surcharge
// multiplier and a flat handling fee, both known at construction time and
// never needing to change out from under a caller mid-calculation. A
// plain, copyable value is the correct level of abstraction for exactly
// that shape of data; nothing here varies polymorphically, so nothing
// here needs an interface.
struct RateSettings {
    double surcharge_multiplier = 1.0;
    double handling_fee = 0.0;
};

// PricingResult carries both facts a caller might want, the total cost and
// whether a surcharge applied, on the same value. This is the direct fix
// for the leaky abstraction in ShipmentOperationsManager:
// was_last_calculation_surcharge_applied() answered a question about the
// manager's internal state. surcharge_applied answers the same question
// about this specific result, because it lives on the result.
struct PricingResult {
    double total_cost = 0.0;
    bool surcharge_applied = false;
};

// ShipmentPricingService replaces ShippingRateRegistry, RateCalculator,
// StandardRateCalculator, RateCalculatorFactory,
// StandardRateCalculatorFactory, and RateCalculatorFactoryProvider with one
// type and one constructor parameter. The settings are injected once, by
// value, by whatever composition root builds the service; nothing is
// reached through a global accessor, and nothing is resolved through an
// interface with a single implementation behind it.
class ShipmentPricingService {
public:
    explicit ShipmentPricingService(RateSettings settings) : settings_(settings) {}

    PricingResult price(double base_cost) const;

private:
    RateSettings settings_;
};

}  // namespace antipattern26

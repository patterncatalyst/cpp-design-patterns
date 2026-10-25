#pragma once

// Encapsulate Global References. The inherited pricing module reads a
// fuel-surcharge multiplier directly, inline, from a mutable global set
// once at startup by an operations console nothing in this chapter owns.
// This technique does not remove the global; legacy_global still exists
// below, exactly as before. It wraps the read behind an interface, so new
// pricing code depends on FuelSurchargeSource instead of calling
// legacy_global::current_fuel_surcharge() inline wherever it needs a rate.

#include <string>

namespace breakdep28 {

namespace legacy_global {

extern double g_fuel_surcharge_multiplier;

double current_fuel_surcharge();
void set_fuel_surcharge(double multiplier);

}  // namespace legacy_global

// The seam: pricing code depends on this interface instead of the global.
class FuelSurchargeSource {
public:
    virtual ~FuelSurchargeSource() = default;
    virtual double multiplier() const = 0;
};

// Production wiring: delegates straight through to the legacy global, so
// live behavior is unchanged from before this technique was applied.
class LiveFuelSurchargeSource : public FuelSurchargeSource {
public:
    double multiplier() const override;
};

class ShipmentCostCalculator {
public:
    explicit ShipmentCostCalculator(const FuelSurchargeSource& source) : source_(source) {}
    double total_cost(double base_cost) const;

private:
    const FuelSurchargeSource& source_;
};

}  // namespace breakdep28

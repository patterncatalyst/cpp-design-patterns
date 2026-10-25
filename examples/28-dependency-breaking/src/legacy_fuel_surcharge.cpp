#include "legacy_fuel_surcharge.hpp"

namespace breakdep28 {

namespace legacy_global {

double g_fuel_surcharge_multiplier = 1.0;

double current_fuel_surcharge() { return g_fuel_surcharge_multiplier; }
void set_fuel_surcharge(double multiplier) { g_fuel_surcharge_multiplier = multiplier; }

}  // namespace legacy_global

double LiveFuelSurchargeSource::multiplier() const { return legacy_global::current_fuel_surcharge(); }

double ShipmentCostCalculator::total_cost(double base_cost) const { return base_cost * source_.multiplier(); }

}  // namespace breakdep28

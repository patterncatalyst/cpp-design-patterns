#include "flat_rate_strategy.hpp"

namespace finalproject31 {

FlatRateStrategy::FlatRateStrategy(Cents flat_fee_cents) : flat_fee_cents_(flat_fee_cents) {}

Cents FlatRateStrategy::quote(const Shipment& /*shipment*/) const { return flat_fee_cents_; }

}  // namespace finalproject31

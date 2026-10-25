#pragma once

#include "pricing_engine.hpp"

namespace strangler30 {

// A single, explicit customer adjustment, replacing LegacyPricingEngine's
// anonymous if/else chain. The two contract terms below were reverse
// engineered from LegacyPricingEngine::apply_customer_overrides and
// confirmed correct only by passing the shared characterization table in
// tests/pricing_characterization_cases.hpp; no contract document for
// either one was found while building this replacement.
struct CustomerAdjustment {
    enum class Kind { FlatCentsOffAboveThreshold, PercentOff };
    Kind kind = Kind::PercentOff;
    double value = 0.0;
    int threshold_cents = 0;
};

// The bounded rewrite's replacement for LegacyPricingEngine. Same public
// contract (PricingEngine), same quoted totals for every shipment profile
// the characterization suite covers, different internals: a named zone
// rate card instead of an inline matrix, a named adjustment table instead
// of an if/else chain, and the mainframe rounding rule kept, not deleted,
// because nothing in this rewrite confirmed it is safe to drop.
class ModernPricingEngine : public PricingEngine {
public:
    Quote quote(const ShipmentRequest& request) const override;

private:
    int zone_base_cents(const std::string& origin, const std::string& destination) const;
    int apply_customer_adjustment(const std::string& customer_id, int subtotal_cents) const;
};

}  // namespace strangler30

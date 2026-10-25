#pragma once

#include "pricing_engine.hpp"

namespace strangler30 {

// The component this chapter is deciding whether to refactor or replace.
// Three facts live inside this class that are not written down anywhere
// else: a zone-to-zone base-rate matrix grown one lane negotiation at a
// time, two customer-specific contract discounts applied as raw
// arithmetic, and a final round-up-to-the-quarter-dollar step nobody
// currently on the team can confirm the invoicing system still requires.
// None of the three is documented outside this file; the characterization
// table in tests/pricing_characterization_cases.hpp is the only durable
// record of what this class actually does.
class LegacyPricingEngine : public PricingEngine {
public:
    Quote quote(const ShipmentRequest& request) const override;

private:
    int zone_base_cents(const std::string& origin, const std::string& destination) const;
    int apply_customer_overrides(const std::string& customer_id, int subtotal_cents) const;
};

}  // namespace strangler30

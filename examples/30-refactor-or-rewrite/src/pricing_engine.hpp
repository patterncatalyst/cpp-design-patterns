#pragma once

#include <stdexcept>
#include <string>

#if __cplusplus >= 202302L
#include <expected>
#endif

namespace strangler30 {

// One freight shipment to be priced. customer_id drives the two
// contract-specific overrides buried in LegacyPricingEngine; origin_zone
// and destination_zone select a lane out of the carrier's five regional
// hubs.
struct ShipmentRequest {
    std::string customer_id;
    std::string origin_zone;
    std::string destination_zone;
    double weight_kg = 0.0;
    int declared_value_cents = 0;
};

struct Quote {
    int total_cents = 0;
    std::string rationale;
};

enum class PricingError {
    NegativeWeight,
    UnknownZonePair,
};

// The stable interface the strangler fig seam is built on. Checkout and
// billing code depends on PricingEngine, never on LegacyPricingEngine or
// ModernPricingEngine directly, so a router that chooses between the two
// at runtime can stand behind this type without either concrete engine,
// or a single caller, ever changing.
class PricingEngine {
public:
    virtual ~PricingEngine() = default;

    // Throws std::invalid_argument for a negative weight or an
    // unrecognized zone pair. Every concrete engine, legacy, modern, and
    // the router that chooses between them, is held to this one contract.
    virtual Quote quote(const ShipmentRequest& request) const = 0;

#if __cplusplus >= 202302L
    // Non-virtual interface: a caller built against this base class gets a
    // typed-rejection call for free under C++23, with zero changes to
    // LegacyPricingEngine or ModernPricingEngine. Neither override knows
    // std::expected exists; the stability of PricingEngine is exactly what
    // makes that possible.
    std::expected<Quote, PricingError> try_quote(const ShipmentRequest& request) const {
        if (request.weight_kg < 0.0) {
            return std::unexpected(PricingError::NegativeWeight);
        }
        try {
            return quote(request);
        } catch (const std::invalid_argument&) {
            return std::unexpected(PricingError::UnknownZonePair);
        }
    }
#endif
};

}  // namespace strangler30

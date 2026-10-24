#include "quote_domain.hpp"

#include <cmath>

namespace freight {

namespace {

constexpr Cents kBaseCents = 500;
constexpr Cents kPerKgCents = 150;

Cents zone_surcharge_cents(ServiceZone zone) {
    switch (zone) {
        case ServiceZone::Local:
            return 0;
        case ServiceZone::Regional:
            return 200;
        case ServiceZone::National:
            return 500;
        case ServiceZone::International:
            return 1500;
        case ServiceZone::Unassigned:
            return 0;  // never reached: validate_shipment() rejects this zone first
    }
    return 0;
}

}  // namespace

ValidationOutcome validate_shipment(const ShipmentRequest& request) {
    if (request.weight_kg <= 0.0) {
        return ValidationOutcome::RejectedNegativeWeight;
    }
    if (request.zone == ServiceZone::Unassigned) {
        return ValidationOutcome::RejectedMissingZone;
    }
    if (request.declared_value_cents > kMaxInsurableValueCents) {
        return ValidationOutcome::RejectedDeclaredValueTooHigh;
    }
    return ValidationOutcome::Accepted;
}

Cents quote_cost(const ShipmentRequest& request) {
    FREIGHT_REQUIRE(request.weight_kg >= 0.0, "quote_cost: weight_kg must be non-negative");
    const Cents weight_cents = static_cast<Cents>(std::ceil(request.weight_kg * static_cast<double>(kPerKgCents)));
    return kBaseCents + weight_cents + zone_surcharge_cents(request.zone);
}

std::string_view describe(ValidationOutcome outcome) {
    switch (outcome) {
        case ValidationOutcome::Accepted:
            return "accepted";
        case ValidationOutcome::RejectedNegativeWeight:
            return "rejected: weight_kg must be positive";
        case ValidationOutcome::RejectedMissingZone:
            return "rejected: destination zone is not set";
        case ValidationOutcome::RejectedDeclaredValueTooHigh:
            return "rejected: declared value exceeds the insurable limit";
    }
    return "rejected: unknown reason";
}

}  // namespace freight

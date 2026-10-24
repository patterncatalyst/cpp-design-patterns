#pragma once

#include "legacy_rate_api.hpp"
#include "shipping_domain.hpp"

namespace shipping {

// Target interface. Checkout code, and the Facade built later in this
// example, depend on this and nothing else: no carrier name, no legacy
// units, no mention of GlobalFreight anywhere.
class RateSource {
public:
    virtual ~RateSource() = default;
    virtual Cents quote(const Shipment& shipment) const = 0;
};

// Object Adapter: composes a LegacyGlobalFreightRateApi and translates
// between this book's domain types and the legacy shape (kilograms to
// grams, DestinationZone to a numeric zone code) on every call. Composition
// rather than inheritance is what lets this adapter wrap a type this
// example never wrote and never gets to change.
class LegacyFreightRateAdapter : public RateSource {
public:
    explicit LegacyFreightRateAdapter(legacy_freight::LegacyGlobalFreightRateApi legacy_api);

    Cents quote(const Shipment& shipment) const override;

private:
    legacy_freight::LegacyGlobalFreightRateApi legacy_api_;
};

int zone_to_legacy_code(DestinationZone zone);

}  // namespace shipping

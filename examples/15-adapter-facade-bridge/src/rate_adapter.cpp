#include "rate_adapter.hpp"

#include <utility>

namespace shipping {

LegacyFreightRateAdapter::LegacyFreightRateAdapter(legacy_freight::LegacyGlobalFreightRateApi legacy_api)
    : legacy_api_(std::move(legacy_api)) {}

int zone_to_legacy_code(DestinationZone zone) {
    switch (zone) {
        case DestinationZone::Local:
            return 0;
        case DestinationZone::Regional:
            return 1;
        case DestinationZone::National:
            return 2;
        case DestinationZone::International:
            return 3;
    }
    return 0;
}

Cents LegacyFreightRateAdapter::quote(const Shipment& shipment) const {
    const long weight_grams = static_cast<long>(shipment.weight_kg * 1000.0);
    const int zone_code = zone_to_legacy_code(shipment.zone);
    return legacy_api_.lookup_rate_cents_for_parcel(weight_grams, zone_code);
}

}  // namespace shipping

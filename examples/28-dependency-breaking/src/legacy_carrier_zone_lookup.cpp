#include "legacy_carrier_zone_lookup.hpp"

namespace breakdep28 {

int LegacyCarrierZoneLookup::zone_for(const std::string& postal_code) {
    if (postal_code.empty()) {
        return 0;
    }
    const char leading = postal_code.front();
    if (leading >= '0' && leading <= '3') {
        return 1;
    }
    if (leading >= '4' && leading <= '6') {
        return 2;
    }
    return 3;
}

std::string ZoneSurchargeReport::describe(const std::string& postal_code) {
    const int zone = LegacyCarrierZoneLookup::zone_for(postal_code);
    return "zone " + std::to_string(zone) + " surcharge report for " + postal_code;
}

int CarrierZoneLookupDelegator::zone_for(const std::string& postal_code) const {
    return LegacyCarrierZoneLookup::zone_for(postal_code);
}

std::string ShipmentRoutingAdvisor::route_hint(const std::string& postal_code) const {
    const int zone = lookup_.zone_for(postal_code);
    if (zone <= 1) {
        return "local hub, zone " + std::to_string(zone);
    }
    if (zone == 2) {
        return "regional hub, zone " + std::to_string(zone);
    }
    return "long-haul hub, zone " + std::to_string(zone);
}

}  // namespace breakdep28

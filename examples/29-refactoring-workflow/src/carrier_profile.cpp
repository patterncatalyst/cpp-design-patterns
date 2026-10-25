#include "carrier_profile.hpp"

namespace refactor29 {

int GroundCarrierProfile::base_transit_days(const ShipmentProfile& shipment) const {
    return (shipment.origin_zone == shipment.destination_zone) ? 2 : 5;
}

int AirCarrierProfile::base_transit_days(const ShipmentProfile& shipment) const {
    return (shipment.origin_zone == shipment.destination_zone) ? 1 : 2;
}

int ExpressCarrierProfile::base_transit_days(const ShipmentProfile&) const {
    return 1;
}

int FreightCarrierProfile::base_transit_days(const ShipmentProfile&) const {
    return 6;
}

std::unique_ptr<CarrierProfile> carrier_profile_for(const std::string& carrier) {
    if (carrier == "air") {
        return std::make_unique<AirCarrierProfile>();
    }
    if (carrier == "express") {
        return std::make_unique<ExpressCarrierProfile>();
    }
    if (carrier == "freight") {
        return std::make_unique<FreightCarrierProfile>();
    }
    return std::make_unique<GroundCarrierProfile>();  // ground, and the original chain's fallback
}

}  // namespace refactor29

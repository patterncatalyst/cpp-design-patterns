#pragma once

#include <memory>
#include <string>

#include "shipment_profile.hpp"

namespace refactor29 {

// Replace Conditional with Polymorphism: each carrier owns its own
// transit-day rule instead of being one more branch in a shared if/else
// chain. Adding a fourth carrier means writing one new class, not editing
// the three that already exist.
class CarrierProfile {
public:
    virtual ~CarrierProfile() = default;
    virtual int base_transit_days(const ShipmentProfile& shipment) const = 0;
};

class GroundCarrierProfile final : public CarrierProfile {
public:
    int base_transit_days(const ShipmentProfile& shipment) const override;
};

class AirCarrierProfile final : public CarrierProfile {
public:
    int base_transit_days(const ShipmentProfile& shipment) const override;
};

class ExpressCarrierProfile final : public CarrierProfile {
public:
    int base_transit_days(const ShipmentProfile& shipment) const override;
};

// A fourth carrier, added after the if/else chain was already retired.
// Nothing above this class changed to make room for it.
class FreightCarrierProfile final : public CarrierProfile {
public:
    int base_transit_days(const ShipmentProfile& shipment) const override;
};

// Looks up the profile for a carrier name; an unrecognized name falls
// back to GroundCarrierProfile, the same default the original if/else
// chain used.
std::unique_ptr<CarrierProfile> carrier_profile_for(const std::string& carrier);

}  // namespace refactor29

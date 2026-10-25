#pragma once

// Introduce Instance Delegator. LegacyCarrierZoneLookup is a static-only
// utility called directly from many places elsewhere in the inherited
// codebase; ZoneSurchargeReport below stands in for one of those existing
// call sites. Changing LegacyCarrierZoneLookup's static interface, or
// replacing it with an instance-based design outright, would mean
// touching every one of those call sites in the same change. Introducing
// a thin instance delegator gives new code an injectable, substitutable
// collaborator without changing LegacyCarrierZoneLookup or any caller
// that already depends on its static method.

#include <string>

namespace breakdep28 {

class LegacyCarrierZoneLookup {
public:
    LegacyCarrierZoneLookup() = delete;
    static int zone_for(const std::string& postal_code);
};

// A pre-existing legacy caller, left exactly as it was: it calls the
// static method directly and keeps working, untouched by anything below.
class ZoneSurchargeReport {
public:
    static std::string describe(const std::string& postal_code);
};

// The seam new code depends on instead of the static method.
class ZoneLookupPort {
public:
    virtual ~ZoneLookupPort() = default;
    virtual int zone_for(const std::string& postal_code) const = 0;
};

// The delegator: a thin instance that forwards to the static legacy
// method. It adds nothing to the lookup logic itself; its only job is
// being an object new code can hold a reference to and a test can replace.
class CarrierZoneLookupDelegator : public ZoneLookupPort {
public:
    int zone_for(const std::string& postal_code) const override;
};

// New code, written against the port from the start.
class ShipmentRoutingAdvisor {
public:
    explicit ShipmentRoutingAdvisor(const ZoneLookupPort& lookup) : lookup_(lookup) {}
    std::string route_hint(const std::string& postal_code) const;

private:
    const ZoneLookupPort& lookup_;
};

}  // namespace breakdep28

#pragma once

#include <cstdint>
#include <string>

namespace carrier_integration {

// Money in this domain is tracked as integer cents, never floating point,
// so a rate quote is exact and reproducible in a test assertion.
using Cents = std::int64_t;

// A parcel ready to hand to whichever carrier's family of objects
// processes it. Both the Abstract Factory and the Factory Method examples
// in this directory share this one domain type.
struct Shipment {
    std::string destination_country;
    double weight_kg;
    Cents declared_value_cents;
};

// A merchant's account with one carrier, read from configuration at
// startup. CarrierOnboarder (Factory Method, in factory_method.hpp) uses
// this to decide which integration test suite an account must pass before
// it can ship live traffic.
struct CarrierAccount {
    std::string carrier_name;
    std::string account_id;
    bool webhook_registered;
};

}  // namespace carrier_integration

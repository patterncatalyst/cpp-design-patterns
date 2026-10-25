#pragma once

#include <memory>
#include <string>

#include "rate_strategy.hpp"

#if __cplusplus >= 202302L
#include <expected>
#endif

namespace finalproject31 {

// What a caller provides to get a carrier: a code identifying which
// carrier contract applies, and the destination zone the quote needs to
// account for.
struct CarrierRequest {
    std::string carrier_code;
    std::string destination_zone;
};

// What CarrierFactory builds: a display name, a service-level commitment
// in hours, and the RateStrategy that carrier's contract calls for. The
// three always arrive together from one factory call, so a caller can
// never end up with a label naming one carrier and a rate strategy
// belonging to another.
struct CarrierProfile {
    std::string carrier_name;
    int sla_hours;
    std::unique_ptr<RateStrategy> rate_strategy;
};

enum class CarrierFactoryError {
    UnknownCarrierCode,
};

std::string to_string(CarrierFactoryError error);

// Factory: client code that needs a carrier's components should never
// have to know, or name, which concrete RateStrategy backs that carrier.
// CarrierFactory is where "SKYHAUL" becomes a WeightTieredRateStrategy and
// "GROUNDLINE" becomes a FlatRateStrategy; every other component in this
// system only ever sees the RateStrategy interface the factory handed
// back.
class CarrierFactory {
public:
    // Throws std::invalid_argument for a carrier_code this factory does
    // not recognize.
    CarrierProfile create(const CarrierRequest& request) const;

#if __cplusplus >= 202302L
    // C++23: the same lookup, implemented once on top of create(), with a
    // typed error a caller can branch on instead of catching an exception.
    std::expected<CarrierProfile, CarrierFactoryError> try_create(const CarrierRequest& request) const;
#endif
};

}  // namespace finalproject31

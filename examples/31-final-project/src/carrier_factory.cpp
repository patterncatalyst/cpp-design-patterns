#include "carrier_factory.hpp"

#include <stdexcept>

#include "flat_rate_strategy.hpp"
#include "weight_tiered_rate_strategy.hpp"

namespace finalproject31 {

std::string to_string(CarrierFactoryError error) {
    switch (error) {
        case CarrierFactoryError::UnknownCarrierCode:
            return "UnknownCarrierCode";
    }
    return "Unknown";
}

CarrierProfile CarrierFactory::create(const CarrierRequest& request) const {
    if (request.carrier_code == "GROUNDLINE") {
        return CarrierProfile{
            .carrier_name = "Groundline Parcel",
            .sla_hours = 72,
            .rate_strategy = std::make_unique<FlatRateStrategy>(899),
        };
    }
    if (request.carrier_code == "SKYHAUL") {
        return CarrierProfile{
            .carrier_name = "Skyhaul Express",
            .sla_hours = 24,
            .rate_strategy = std::make_unique<WeightTieredRateStrategy>(std::vector<WeightTier>{
                WeightTier{.max_weight_kg = 5.0, .base_cents = 1200, .cents_per_kg = 180.0},
                WeightTier{.max_weight_kg = 20.0, .base_cents = 1600, .cents_per_kg = 140.0},
                WeightTier{.max_weight_kg = 50.0, .base_cents = 2400, .cents_per_kg = 95.0},
            }),
        };
    }

    throw std::invalid_argument("CarrierFactory: unrecognized carrier_code '" + request.carrier_code + "'");
}

#if __cplusplus >= 202302L
std::expected<CarrierProfile, CarrierFactoryError> CarrierFactory::try_create(const CarrierRequest& request) const {
    try {
        return create(request);
    } catch (const std::invalid_argument&) {
        return std::unexpected(CarrierFactoryError::UnknownCarrierCode);
    }
}
#endif

}  // namespace finalproject31

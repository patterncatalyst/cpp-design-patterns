#include "carrier_policy.hpp"

namespace exprtmpl {
namespace {

// Nothing outside this file ever names FlatRateCarrierPolicy. The only way
// another translation unit can reach it is through the registry, under the
// "flat-rate" key the registrar below installs.
class FlatRateCarrierPolicy : public CarrierPolicy {
public:
    std::unique_ptr<CarrierPolicy> clone() const override {
        return std::make_unique<FlatRateCarrierPolicy>(*this);
    }

    std::string_view name() const override { return "flat-rate"; }
    double base_rate_multiplier() const override { return 1.0; }
};

const CarrierPolicyRegistrar<FlatRateCarrierPolicy> registrar{"flat-rate"};

}  // namespace
}  // namespace exprtmpl

#include "carrier_policy.hpp"

namespace exprtmpl {
namespace {

// This is the type examples/23-expression-templates/tests/carrier_policy_registry_test.cpp
// resolves by name alone: the test never includes this file or spells
// WeightTieredCarrierPolicy anywhere, it only asks the registry for
// "weight-tiered" and gets back a clone of what this TU registered.
class WeightTieredCarrierPolicy : public CarrierPolicy {
public:
    std::unique_ptr<CarrierPolicy> clone() const override {
        return std::make_unique<WeightTieredCarrierPolicy>(*this);
    }

    std::string_view name() const override { return "weight-tiered"; }
    double base_rate_multiplier() const override { return 1.35; }
};

const CarrierPolicyRegistrar<WeightTieredCarrierPolicy> registrar{"weight-tiered"};

}  // namespace
}  // namespace exprtmpl

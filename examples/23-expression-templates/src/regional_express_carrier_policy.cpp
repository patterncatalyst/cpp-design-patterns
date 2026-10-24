#include "carrier_policy.hpp"

namespace exprtmpl {
namespace {

class RegionalExpressCarrierPolicy : public CarrierPolicy {
public:
    std::unique_ptr<CarrierPolicy> clone() const override {
        return std::make_unique<RegionalExpressCarrierPolicy>(*this);
    }

    std::string_view name() const override { return "regional-express"; }
    double base_rate_multiplier() const override { return 1.8; }
};

const CarrierPolicyRegistrar<RegionalExpressCarrierPolicy> registrar{"regional-express"};

}  // namespace
}  // namespace exprtmpl

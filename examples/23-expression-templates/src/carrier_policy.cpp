#include "carrier_policy.hpp"

#include <utility>

namespace exprtmpl {

CarrierPolicyRegistry& CarrierPolicyRegistry::instance() {
    static CarrierPolicyRegistry registry;
    return registry;
}

void CarrierPolicyRegistry::register_prototype(std::string key, std::unique_ptr<CarrierPolicy> prototype) {
    prototypes_[std::move(key)] = std::move(prototype);
}

std::unique_ptr<CarrierPolicy> CarrierPolicyRegistry::create(const std::string& key) const {
    const auto it = prototypes_.find(key);
    if (it == prototypes_.end()) {
        return nullptr;
    }
    return it->second->clone();
}

std::size_t CarrierPolicyRegistry::size() const { return prototypes_.size(); }

#if __cplusplus >= 202302L
std::expected<std::unique_ptr<CarrierPolicy>, std::string> CarrierPolicyRegistry::try_create(
    const std::string& key) const {
    if (key.empty()) {
        return std::unexpected<std::string>("carrier policy key must not be empty");
    }
    const auto it = prototypes_.find(key);
    if (it == prototypes_.end()) {
        return std::unexpected<std::string>("no carrier policy registered for key \"" + key + "\"");
    }
    return it->second->clone();
}
#endif

}  // namespace exprtmpl

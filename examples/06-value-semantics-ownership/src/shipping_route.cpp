#include "shipping_route.hpp"

#include <utility>

namespace warehouse {

ShippingRoute::ShippingRoute(std::string route_name, std::shared_ptr<CarrierContract> contract, std::size_t tier)
    : route_name_(std::move(route_name)), contract_(std::move(contract)), tier_(tier) {}

Cents ShippingRoute::quote() const {
    return contract_->tier_rate(tier_);
}

const std::string& ShippingRoute::route_name() const {
    return route_name_;
}

const std::shared_ptr<CarrierContract>& ShippingRoute::contract() const {
    return contract_;
}

}  // namespace warehouse

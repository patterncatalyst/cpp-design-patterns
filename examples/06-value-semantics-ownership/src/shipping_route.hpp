#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "carrier_contract.hpp"

namespace warehouse {

// A route quotes against a carrier contract it does not own outright. More
// than one route can be negotiated under the same contract (a regional
// carrier sells the same tier table to every route it serves), so the
// route holds its contract through a std::shared_ptr: the contract stays
// alive as long as at least one route still references it, and every
// route sharing the pointer sees the same contract object, not a copy of
// it.
class ShippingRoute {
public:
    ShippingRoute(std::string route_name, std::shared_ptr<CarrierContract> contract, std::size_t tier);

    Cents quote() const;
    const std::string& route_name() const;
    const std::shared_ptr<CarrierContract>& contract() const;

private:
    std::string route_name_;
    std::shared_ptr<CarrierContract> contract_;
    std::size_t tier_;
};

}  // namespace warehouse

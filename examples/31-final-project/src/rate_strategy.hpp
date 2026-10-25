#pragma once

#include "shipment.hpp"
#include "shipment_types.hpp"

namespace finalproject31 {

// Strategy: the algorithm that computes a shipment's cost varies by
// carrier contract, and nothing else in this system should need to know
// which rule is in effect to ask for a quote.
class RateStrategy {
public:
    virtual ~RateStrategy() = default;

    virtual Cents quote(const Shipment& shipment) const = 0;
};

}  // namespace finalproject31

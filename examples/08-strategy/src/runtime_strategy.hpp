#pragma once

#include <memory>

#include "shipping_domain.hpp"

#if __cplusplus >= 202302L
#include <expected>
#include <string>
#endif

namespace shipping {

// Strategy expression 1: runtime polymorphism. RateStrategy is the
// interface every carrier implements; RateEstimator owns exactly one
// carrier at a time as a std::unique_ptr, injected at construction and
// swappable later, so a different carrier can be selected at runtime
// without recompiling or touching the estimator's own code.
class RateStrategy {
public:
    virtual ~RateStrategy() = default;
    virtual Cents quote(const Shipment& shipment) const = 0;
};

class FlatRateCarrier : public RateStrategy {
public:
    explicit FlatRateCarrier(Cents flat_cents);
    Cents quote(const Shipment& shipment) const override;

private:
    Cents flat_cents_;
};

class WeightBasedCarrier : public RateStrategy {
public:
    WeightBasedCarrier(Cents base_cents, Cents per_kg_cents);
    Cents quote(const Shipment& shipment) const override;

private:
    Cents base_cents_;
    Cents per_kg_cents_;
};

class TieredZoneCarrier : public RateStrategy {
public:
    Cents quote(const Shipment& shipment) const override;
};

class RateEstimator {
public:
    explicit RateEstimator(std::unique_ptr<RateStrategy> strategy);

    // Swap carriers at runtime, e.g. when a customer picks a different
    // shipping option at checkout.
    void use_strategy(std::unique_ptr<RateStrategy> strategy);

    Cents estimate(const Shipment& shipment) const;

#if __cplusplus >= 202302L
    // C++23 refinement: reject a malformed shipment (negative weight) with
    // a typed error instead of a sentinel value or an exception. Guarded
    // so the mandatory C++20 build path never requires std::expected.
    std::expected<Cents, std::string> try_estimate(const Shipment& shipment) const;
#endif

private:
    std::unique_ptr<RateStrategy> strategy_;
};

}  // namespace shipping

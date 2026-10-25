#pragma once

namespace antipattern26 {

// RateCalculator is an abstraction with exactly one implementation,
// StandardRateCalculator, and no second implementation anywhere in this
// codebase, no ticket asking for one, and no plausible one on the horizon.
// That is misuse #3, speculative generality: an interface built for a
// variation point nobody has ever needed.
class RateCalculator {
public:
    virtual ~RateCalculator() = default;
    virtual double calculate(double base_cost) const = 0;
};

// The only implementation. It does not even hold the surcharge multiplier
// itself; it reaches into ShippingRateRegistry::instance() every time
// calculate() runs, which is what makes this type's behavior depend on
// whatever any other part of the program last wrote into that singleton.
class StandardRateCalculator : public RateCalculator {
public:
    double calculate(double base_cost) const override;
};

}  // namespace antipattern26

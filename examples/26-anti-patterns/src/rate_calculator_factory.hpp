#pragma once

#include <memory>

#include "rate_calculator.hpp"

namespace antipattern26 {

// RateCalculatorFactory is a second abstraction wrapped around the first
// one. Like RateCalculator itself, it has exactly one implementation,
// StandardRateCalculatorFactory, whose create_calculator() does nothing
// except call std::make_unique<StandardRateCalculator>(). Factory Method
// earns its keep when choosing which concrete type to build is itself a
// decision, resolved by a parameter, a config value, or a registered
// family of types. Nothing here varies: this factory always returns the
// same concrete type, so the "factory" is a constructor call wearing an
// interface.
class RateCalculatorFactory {
public:
    virtual ~RateCalculatorFactory() = default;
    virtual std::unique_ptr<RateCalculator> create_calculator() const = 0;
};

class StandardRateCalculatorFactory : public RateCalculatorFactory {
public:
    std::unique_ptr<RateCalculator> create_calculator() const override;
};

// RateCalculatorFactoryProvider is the second singleton in this design,
// and it is what turns "one unnecessary factory" into "a factory of
// factories": a globally reachable accessor whose only job is handing out
// the one RateCalculatorFactory this program will ever construct. A caller
// three function calls away from main() can reach RateCalculatorFactory
// without anyone passing it a reference, which is convenient right up
// until a test needs to see what that caller actually depends on.
class RateCalculatorFactoryProvider {
public:
    static RateCalculatorFactoryProvider& instance();

    RateCalculatorFactoryProvider(const RateCalculatorFactoryProvider&) = delete;
    RateCalculatorFactoryProvider& operator=(const RateCalculatorFactoryProvider&) = delete;

    RateCalculatorFactory& factory();

private:
    RateCalculatorFactoryProvider();

    std::unique_ptr<RateCalculatorFactory> factory_;
};

}  // namespace antipattern26

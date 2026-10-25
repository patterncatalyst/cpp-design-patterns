#include "rate_calculator_factory.hpp"

namespace antipattern26 {

std::unique_ptr<RateCalculator> StandardRateCalculatorFactory::create_calculator() const {
    return std::make_unique<StandardRateCalculator>();
}

RateCalculatorFactoryProvider::RateCalculatorFactoryProvider()
    : factory_(std::make_unique<StandardRateCalculatorFactory>()) {}

RateCalculatorFactoryProvider& RateCalculatorFactoryProvider::instance() {
    static RateCalculatorFactoryProvider provider;
    return provider;
}

RateCalculatorFactory& RateCalculatorFactoryProvider::factory() {
    return *factory_;
}

}  // namespace antipattern26

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "shipping_domain.hpp"

namespace carrier_integration {

// Factory Method's product: a single integration test suite run against a
// carrier account before it goes live. Factory Method varies which one
// concrete suite gets created; unlike the Abstract Factory above, it never
// produces a family of several related objects at once.
class IntegrationTestSuite {
public:
    virtual ~IntegrationTestSuite() = default;
    virtual std::vector<std::string> run(const CarrierAccount& account) const = 0;
};

class SandboxTestSuite : public IntegrationTestSuite {
public:
    std::vector<std::string> run(const CarrierAccount& account) const override;
};

class ProductionSmokeTestSuite : public IntegrationTestSuite {
public:
    std::vector<std::string> run(const CarrierAccount& account) const override;
};

struct OnboardingResult {
    bool passed;
    std::vector<std::string> log;
};

// Factory Method: onboard() is a fixed sequence, validate the account, run
// whichever test suite create_test_suite() returns, then judge the
// outcome. create_test_suite() is the one step a subclass decides; nothing
// else about onboarding changes between a sandbox account and a production
// one.
class CarrierOnboarder {
public:
    virtual ~CarrierOnboarder() = default;
    OnboardingResult onboard(const CarrierAccount& account) const;

private:
    // The factory method. Private, not protected: a subclass supplies the
    // product it creates but has no way to call this directly from
    // anywhere outside onboard()'s own fixed sequence.
    virtual std::unique_ptr<IntegrationTestSuite> create_test_suite() const = 0;
};

class SandboxOnboarder : public CarrierOnboarder {
private:
    std::unique_ptr<IntegrationTestSuite> create_test_suite() const override;
};

class ProductionOnboarder : public CarrierOnboarder {
private:
    std::unique_ptr<IntegrationTestSuite> create_test_suite() const override;
};

}  // namespace carrier_integration

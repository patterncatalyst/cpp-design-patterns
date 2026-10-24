#include "factory_method.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

namespace carrier_integration {

std::vector<std::string> SandboxTestSuite::run(const CarrierAccount& account) const {
    return {
        "sandbox: validated account id " + account.account_id,
        "sandbox: accepted a synthetic test shipment",
        "sandbox: skipped webhook delivery check (not required in sandbox)",
    };
}

std::vector<std::string> ProductionSmokeTestSuite::run(const CarrierAccount& account) const {
    std::vector<std::string> log;
    log.push_back("production: validated account id " + account.account_id);
    log.push_back("production: quoted a live test shipment");
    if (account.webhook_registered) {
        log.push_back("production: confirmed webhook delivery");
    } else {
        log.push_back("production: FAILED webhook delivery check, no webhook registered");
    }
    return log;
}

OnboardingResult CarrierOnboarder::onboard(const CarrierAccount& account) const {
    if (account.account_id.empty()) {
        throw std::invalid_argument("carrier account id must not be empty");
    }

    const std::unique_ptr<IntegrationTestSuite> suite = create_test_suite();
    std::vector<std::string> log = suite->run(account);

    const bool passed = std::none_of(log.begin(), log.end(), [](const std::string& line) {
        return line.find("FAILED") != std::string::npos;
    });

    return OnboardingResult{passed, std::move(log)};
}

std::unique_ptr<IntegrationTestSuite> SandboxOnboarder::create_test_suite() const {
    return std::make_unique<SandboxTestSuite>();
}

std::unique_ptr<IntegrationTestSuite> ProductionOnboarder::create_test_suite() const {
    return std::make_unique<ProductionSmokeTestSuite>();
}

}  // namespace carrier_integration

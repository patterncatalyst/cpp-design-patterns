#pragma once

#include <string>
#include <vector>

#include "audit_sink.hpp"

namespace shipping::audit {

// Hand-written test double, the idiomatic manual-DI way to prove
// OrderFulfillmentService only ever talks to the AuditSink abstraction it
// was given. Every test constructs its own FakeAuditSink: there is no
// shared global state to reset between tests, unlike ShipmentAuditLog's
// reset_for_testing() escape hatch.
class FakeAuditSink : public AuditSink {
public:
    bool record(const std::string& event) override {
        recorded_.push_back(event);
        return true;
    }

    const std::vector<std::string>& recorded() const { return recorded_; }

private:
    std::vector<std::string> recorded_;
};

// Simulates a backing store that rejects every write, the scenario this
// chapter's DI version can test directly and the singleton version cannot
// without rebuilding its one compiled-in implementation.
class FailingAuditSink : public AuditSink {
public:
    bool record(const std::string&) override { return false; }
};

}  // namespace shipping::audit

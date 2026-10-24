// Singleton versus dependency injection, side by side: ShipmentAuditLog
// reached as a global singleton, and the identical capability modeled as an
// AuditSink abstraction wired by a composition root. Build and run via
// scripts/build.sh from the repo root, or standalone from this directory:
// see README.md "Build, run, observe".

#include <iostream>
#include <string>

#include "audit_sink.hpp"
#include "order_fulfillment_service.hpp"
#include "shipment_audit_log_singleton.hpp"

namespace {

using shipping::audit::ShipmentAuditLog;

// Two call sites standing in for two unrelated modules, both reaching into
// the same global accessor with nothing in either function's own signature
// announcing the dependency.
void record_from_warehouse_module(const std::string& order_id) {
    ShipmentAuditLog::instance().record("[warehouse] packed " + order_id);
}

void record_from_billing_module(const std::string& order_id) {
    ShipmentAuditLog::instance().record("[billing] charged " + order_id);
}

// A sink whose backing store is always full: every record() call fails,
// the scenario the injected version can test directly and the singleton's
// one hardwired implementation has no seam to simulate.
class FailingAuditSink : public shipping::audit::AuditSink {
public:
    bool record(const std::string&) override { return false; }
};

}  // namespace

int main() {
    std::cout << "== Singleton: ShipmentAuditLog::instance() ==\n";
    ShipmentAuditLog::instance().reset_for_testing();
    record_from_warehouse_module("ORD-2001");
    record_from_billing_module("ORD-2001");
    for (const auto& entry : ShipmentAuditLog::instance().entries()) {
        std::cout << entry << '\n';
    }
    std::cout << "Same instance from both call sites: "
              << (&ShipmentAuditLog::instance() == &ShipmentAuditLog::instance() ? "yes" : "no") << "\n\n";

    std::cout << "== Dependency injection: OrderFulfillmentService ==\n";
    shipping::audit::InMemoryAuditSink sink;
    shipping::audit::OrderFulfillmentService service(sink);
    const auto receipt = service.fulfill_order("ORD-2002");
    std::cout << receipt.audit_message << '\n';
    for (const auto& entry : sink.entries()) {
        std::cout << "sink entry: " << entry << '\n';
    }

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: typed failure via std::expected ==\n";
    FailingAuditSink failing_sink;
    shipping::audit::OrderFulfillmentService failing_service(failing_sink);
    if (const auto result = failing_service.try_fulfill_order("ORD-2003"); !result.has_value()) {
        std::cout << "Rejected: " << result.error() << '\n';
    }
#endif

    return 0;
}

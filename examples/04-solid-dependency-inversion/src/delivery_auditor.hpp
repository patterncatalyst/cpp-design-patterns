#pragma once

#include <string>

#include "gateway_interfaces.hpp"

namespace notify {

// A third high-level policy, depending on exactly one role interface:
// DeliveryStatusProvider. DeliveryAuditor never sends anything and never
// renders a template, so it never depends on MessageSender or
// TemplateRenderer either. A fat "gateway" interface would have forced
// this class to depend on (and a test double to implement) send() and
// render() methods it has no use for; segregating the interface means an
// auditor-only test double can satisfy exactly one method.
class DeliveryAuditor {
public:
    explicit DeliveryAuditor(DeliveryStatusProvider& status_provider) : status_provider_(status_provider) {}

    bool confirmed_delivered(const std::string& dispatch_id) const;

private:
    DeliveryStatusProvider& status_provider_;
};

}  // namespace notify

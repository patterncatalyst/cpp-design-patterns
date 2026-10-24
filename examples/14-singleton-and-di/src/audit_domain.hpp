#pragma once

#include <string>

namespace shipping::audit {

// Shared message formatting, so the singleton path and the dependency-
// injected path record the identical text for the identical event. Only
// the mechanism that reaches the audit capability differs between the two
// versions in this example; the content never does.
std::string format_fulfillment_event(const std::string& order_id);

}  // namespace shipping::audit

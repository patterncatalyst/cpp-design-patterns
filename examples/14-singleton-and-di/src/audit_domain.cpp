#include "audit_domain.hpp"

namespace shipping::audit {

std::string format_fulfillment_event(const std::string& order_id) {
    return "Order " + order_id + " fulfilled";
}

}  // namespace shipping::audit

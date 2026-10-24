#include "order_fulfillment_service.hpp"

#include "audit_domain.hpp"

namespace shipping::audit {

FulfillmentReceipt OrderFulfillmentService::fulfill_order(const std::string& order_id) const {
    const std::string message = format_fulfillment_event(order_id);
    sink_.record(message);
    return FulfillmentReceipt{order_id, message};
}

#if __cplusplus >= 202302L
std::expected<FulfillmentReceipt, std::string> OrderFulfillmentService::try_fulfill_order(
    const std::string& order_id) const {
    const std::string message = format_fulfillment_event(order_id);
    if (!sink_.record(message)) {
        return std::unexpected<std::string>("audit sink failed to record order " + order_id);
    }
    return FulfillmentReceipt{order_id, message};
}
#else
std::optional<FulfillmentReceipt> OrderFulfillmentService::try_fulfill_order(const std::string& order_id) const {
    const std::string message = format_fulfillment_event(order_id);
    if (!sink_.record(message)) {
        return std::nullopt;
    }
    return FulfillmentReceipt{order_id, message};
}
#endif

}  // namespace shipping::audit

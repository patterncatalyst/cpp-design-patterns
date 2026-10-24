#include "order_notification_policy.hpp"

namespace notify {

DispatchResult OrderNotificationPolicy::notify_customer(const OrderEvent& event) const {
    return sender_.send(event);
}

#if __cplusplus >= 202302L
std::expected<DispatchResult, std::string> OrderNotificationPolicy::try_notify_customer(
    const OrderEvent& event) const {
    if (event.customer_contact.empty()) {
        return std::unexpected<std::string>("order event has no customer_contact to notify");
    }
    return sender_.send(event);
}
#else
std::optional<DispatchResult> OrderNotificationPolicy::try_notify_customer(const OrderEvent& event) const {
    if (event.customer_contact.empty()) {
        return std::nullopt;
    }
    return sender_.send(event);
}
#endif

}  // namespace notify

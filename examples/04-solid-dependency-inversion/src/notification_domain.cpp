#include "notification_domain.hpp"

namespace notify {

std::string format_message(const OrderEvent& event) {
    return "[Order " + event.order_id + "] " + event.message_body;
}

}  // namespace notify

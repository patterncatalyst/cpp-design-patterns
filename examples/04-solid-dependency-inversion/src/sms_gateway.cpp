#include "sms_gateway.hpp"

namespace notify {

DispatchResult SmsGateway::send(const OrderEvent& event) const {
    std::string body = format_message(event);
    if (body.size() > kMaxCharacters) {
        body.resize(kMaxCharacters);
    }
    last_sent_body_ = body;
    return DispatchResult{DispatchStatus::Sent, "sms-" + event.order_id};
}

}  // namespace notify

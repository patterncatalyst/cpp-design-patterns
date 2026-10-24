#pragma once

#include <cstddef>

#include "gateway_interfaces.hpp"
#include "notification_domain.hpp"

namespace notify {

// SMS is the narrow channel in this example: a carrier API that can send a
// short plain-text message and nothing else. It implements exactly
// MessageSender. There is no render() to stub out and no status_of() to
// fake, because SmsGateway never declares a dependency on TemplateRenderer
// or DeliveryStatusProvider in the first place. That absence is the
// payoff of segregating the fat interface Chapter 4 opens with: a channel
// that cannot do something is never asked to pretend it can.
class SmsGateway : public MessageSender {
public:
    DispatchResult send(const OrderEvent& event) const override;

    // Test/diagnostic accessor, not part of any role interface: the actual
    // (possibly truncated) body this gateway last handed to its simulated
    // transport. Used to prove send() starts from the same shared
    // format_message() as EmailGateway before applying SMS's own
    // 160-character limit.
    const std::string& last_sent_body() const { return last_sent_body_; }

private:
    static constexpr std::size_t kMaxCharacters = 160;
    mutable std::string last_sent_body_;
};

}  // namespace notify

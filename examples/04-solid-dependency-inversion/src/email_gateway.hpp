#pragma once

#include <string>
#include <unordered_map>

#include "gateway_interfaces.hpp"
#include "notification_domain.hpp"

namespace notify {

// Email is the full-featured channel in this example: it can send, render
// a named template, and report whether a prior dispatch landed. It
// implements all three role interfaces because it genuinely has all three
// capabilities, not because a fat base interface forced it to.
class EmailGateway : public MessageSender, public TemplateRenderer, public DeliveryStatusProvider {
public:
    DispatchResult send(const OrderEvent& event) const override;
    std::string render(const std::string& template_id, const OrderEvent& event) const override;
    DispatchStatus status_of(const std::string& dispatch_id) const override;

    // Test/diagnostic accessor, not part of any role interface: what body
    // did this gateway actually hand to its (simulated) transport for a
    // given dispatch? Used to prove send() goes through the same shared
    // format_message() as SmsGateway, so the two channels never drift on
    // wording.
    std::string body_sent_for(const std::string& dispatch_id) const;

private:
    // Simulates a delivery log a real email provider's API would expose;
    // every dispatch this gateway sends is recorded as Sent. mutable
    // because send() is logically const from a caller's point of view (it
    // does not change the gateway's configuration), but does record state.
    mutable std::unordered_map<std::string, DispatchStatus> dispatch_log_;
    mutable std::unordered_map<std::string, std::string> sent_bodies_;
};

}  // namespace notify

#pragma once

#include <string>
#include <utility>

#include "gateway_interfaces.hpp"
#include "notification_domain.hpp"

namespace notify {

// A second high-level policy, and the Interface Segregation half of this
// chapter's example. TemplatedOrderNotifier depends on exactly the two
// role interfaces it needs, TemplateRenderer and MessageSender, instead of
// one fat "gateway" interface that also carries status-checking methods
// this class never calls. Any pair of concrete objects that together
// satisfy both role interfaces can be wired in here; in this example that
// is always the same EmailGateway instance playing both roles (see
// demo_main.cpp), because SmsGateway never implements TemplateRenderer in
// the first place.
class TemplatedOrderNotifier {
public:
    TemplatedOrderNotifier(TemplateRenderer& renderer, MessageSender& sender, std::string template_id)
        : renderer_(renderer), sender_(sender), template_id_(std::move(template_id)) {}

    DispatchResult notify_customer(const OrderEvent& event) const;

private:
    TemplateRenderer& renderer_;
    MessageSender& sender_;
    std::string template_id_;
};

}  // namespace notify

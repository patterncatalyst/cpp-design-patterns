#pragma once

#include <string>

#include "notification_domain.hpp"

namespace notify {

// Role interface: can this collaborator deliver a message to a customer?
// This is the one capability every channel in this example has, so it is
// the one capability OrderNotificationPolicy (the DIP example below)
// depends on.
class MessageSender {
public:
    virtual ~MessageSender() = default;
    virtual DispatchResult send(const OrderEvent& event) const = 0;
};

// Role interface: can this collaborator report whether a prior dispatch
// actually reached the customer? Not every channel in this example can
// answer that question, so it is kept separate from MessageSender rather
// than bundled into one fat interface every channel would have to satisfy.
class DeliveryStatusProvider {
public:
    virtual ~DeliveryStatusProvider() = default;
    virtual DispatchStatus status_of(const std::string& dispatch_id) const = 0;
};

// Role interface: can this collaborator render a named template against an
// order event before it is sent? Also not universal across channels, so
// also kept separate.
class TemplateRenderer {
public:
    virtual ~TemplateRenderer() = default;
    virtual std::string render(const std::string& template_id, const OrderEvent& event) const = 0;
};

}  // namespace notify

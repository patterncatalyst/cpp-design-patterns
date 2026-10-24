#pragma once

#include <string>
#include <vector>

#include "gateway_interfaces.hpp"
#include "notification_domain.hpp"

namespace notify {

// Hand-written test doubles, the idiomatic manual-DI way to prove a
// high-level policy only ever talks to the abstraction it was given. Each
// fake implements exactly one role interface, mirroring the production
// gateways' own segregation: a test for OrderNotificationPolicy never
// needs a fake that can also render templates or answer status queries.

// Records every event it was asked to send, so a test can assert on what
// the policy under test actually sent, not just that something happened.
class FakeMessageSender : public MessageSender {
public:
    DispatchResult send(const OrderEvent& event) const override {
        sent_events_.push_back(event);
        return DispatchResult{DispatchStatus::Sent, "fake-" + event.order_id};
    }

    const std::vector<OrderEvent>& sent_events() const { return sent_events_; }

private:
    mutable std::vector<OrderEvent> sent_events_;
};

// Returns a deterministic, inspectable string instead of looking anything
// up in a template table, so a test can assert the exact template_id and
// event TemplatedOrderNotifier passed through.
class FakeTemplateRenderer : public TemplateRenderer {
public:
    std::string render(const std::string& template_id, const OrderEvent& event) const override {
        return template_id + ":" + event.order_id;
    }
};

// A canned status, set once at construction, so DeliveryAuditor tests can
// exercise both the confirmed and unconfirmed branches without a real
// gateway's dispatch log.
class FakeDeliveryStatusProvider : public DeliveryStatusProvider {
public:
    explicit FakeDeliveryStatusProvider(DispatchStatus status) : status_(status) {}

    DispatchStatus status_of(const std::string& /*dispatch_id*/) const override { return status_; }

private:
    DispatchStatus status_;
};

}  // namespace notify

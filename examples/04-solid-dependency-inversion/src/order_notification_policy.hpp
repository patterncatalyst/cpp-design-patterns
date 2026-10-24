#pragma once

#include "gateway_interfaces.hpp"
#include "notification_domain.hpp"

#if __cplusplus >= 202302L
#include <expected>
#include <string>
#else
#include <optional>
#endif

namespace notify {

// The high-level policy in this chapter's Dependency Inversion example.
// OrderNotificationPolicy knows one thing: an order event gets sent to a
// customer. It has no idea whether that happens over email, SMS, or a
// channel not yet written, because it depends on MessageSender, the
// abstraction, rather than on any concrete gateway. The abstraction is
// injected through the constructor; nothing in this class ever names
// EmailGateway or SmsGateway.
//
// The dependency is held as a reference, not a std::unique_ptr: ownership
// of the concrete gateway stays with whoever composes the object graph
// (see demo_main.cpp), not with this policy. That is the idiomatic manual
// DI shape for a collaborator that is wired once, at startup, and lives as
// long as the program does; Chapter 8's RateEstimator instead owned its
// strategy through a unique_ptr because that strategy needed to be
// reassigned, and therefore outlived, at runtime.
class OrderNotificationPolicy {
public:
    explicit OrderNotificationPolicy(MessageSender& sender) : sender_(sender) {}

    DispatchResult notify_customer(const OrderEvent& event) const;

#if __cplusplus >= 202302L
    // C++23 refinement: reject an uncontactable event with a typed reason
    // instead of silently handing it to the sender. Guarded so the
    // mandatory C++20 build path never requires std::expected.
    std::expected<DispatchResult, std::string> try_notify_customer(const OrderEvent& event) const;
#else
    // C++20 fallback: optional loses the reason for rejection; the caller
    // learns *that* an event was uncontactable, not *why*.
    std::optional<DispatchResult> try_notify_customer(const OrderEvent& event) const;
#endif

private:
    MessageSender& sender_;
};

}  // namespace notify

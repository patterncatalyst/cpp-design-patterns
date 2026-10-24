// Order notifications at checkout, segregated into focused role
// interfaces (ISP) and injected into high-level policies by constructor
// (DIP) with no container anywhere. This file IS the object graph's
// composition root: the one place that names both the abstractions and
// their concrete implementations, and wires them together. Everything
// else in src/ only ever sees an abstraction.
//
// Build and run via scripts/build.sh from the repo root, or standalone
// from this directory: see README.md "Build, run, observe".

#include <iostream>

#include "delivery_auditor.hpp"
#include "email_gateway.hpp"
#include "notification_domain.hpp"
#include "order_notification_policy.hpp"
#include "sms_gateway.hpp"
#include "templated_order_notifier.hpp"

int main() {
    using namespace notify;

    // --- Composition root: construct every concrete type exactly once. ---
    EmailGateway email_gateway;
    SmsGateway sms_gateway;

    // DIP: OrderNotificationPolicy only ever sees a MessageSender&. Either
    // gateway satisfies that reference; which one is wired in is decided
    // here, not inside the policy.
    OrderNotificationPolicy sms_notifier(sms_gateway);

    // ISP: TemplatedOrderNotifier needs a TemplateRenderer and a
    // MessageSender. email_gateway is passed in twice, once per role,
    // because it is the one concrete type in this example that satisfies
    // both. SmsGateway could never be wired in here: it never implements
    // TemplateRenderer, so the compiler rejects the attempt rather than a
    // test catching it at runtime.
    TemplatedOrderNotifier email_notifier(email_gateway, email_gateway, "order-delayed");

    // ISP again: DeliveryAuditor needs only a DeliveryStatusProvider, so
    // it is wired to email_gateway through that one role, never through
    // MessageSender or TemplateRenderer.
    DeliveryAuditor auditor(email_gateway);

    // --- Use the graph. ---
    const OrderEvent sms_event{"ORD-1001", "+1-555-0100", "Your order has shipped."};
    const DispatchResult sms_result = sms_notifier.notify_customer(sms_event);
    std::cout << "SMS dispatch (" << sms_result.dispatch_id << "): " << sms_gateway.last_sent_body() << '\n';

    const OrderEvent email_event{"ORD-1002", "customer@example.com", "it left the warehouse this morning"};
    const DispatchResult email_result = email_notifier.notify_customer(email_event);
    std::cout << "Email dispatch (" << email_result.dispatch_id
              << "): " << email_gateway.body_sent_for(email_result.dispatch_id) << '\n';

    std::cout << "Audit for " << email_result.dispatch_id << ": "
              << (auditor.confirmed_delivered(email_result.dispatch_id) ? "confirmed delivered" : "not confirmed")
              << '\n';

#if __cplusplus >= 202302L
    std::cout << "\n== C++23 refinement: typed validation via std::expected ==\n";
    const OrderEvent uncontactable{"ORD-1003", "", "no contact on file"};
    if (const auto result = sms_notifier.try_notify_customer(uncontactable); !result.has_value()) {
        std::cout << "Rejected event: " << result.error() << '\n';
    }
#endif

    return 0;
}

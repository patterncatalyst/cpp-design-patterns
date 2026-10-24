#pragma once

#include <string>

namespace notify {

// Whether a dispatch actually left the building. Both gateways in this
// example always succeed (there is no network in a book example), but the
// type exists so a real gateway's retry/backoff logic has somewhere to
// report failure without inventing a new status enum per gateway.
enum class DispatchStatus {
    Sent,
    Failed,
};

// An order-lifecycle event a customer needs to hear about: a shipment
// confirmation, a delay notice, a delivery receipt. customer_contact is an
// email address or a phone number depending on which channel ends up
// sending it; this struct does not know or care which.
struct OrderEvent {
    std::string order_id;
    std::string customer_contact;
    std::string message_body;
};

// What a MessageSender hands back after attempting a dispatch.
// dispatch_id is empty when status is Failed.
struct DispatchResult {
    DispatchStatus status;
    std::string dispatch_id;
};

// Shared formatting so every channel sends byte-identical message content;
// only the transport (email, SMS) differs. This is the same shared-core
// move Chapter 8 used for shipping-rate math: keeping the formatting rule
// in one place means a wording fix lands for every gateway at once, and a
// test comparing gateways for consistency has one fact to assert on.
std::string format_message(const OrderEvent& event);

}  // namespace notify

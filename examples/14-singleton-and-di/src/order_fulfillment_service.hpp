#pragma once

#include <string>

#include "audit_sink.hpp"

#if __cplusplus >= 202302L
#include <expected>
#else
#include <optional>
#endif

namespace shipping::audit {

struct FulfillmentReceipt {
    std::string order_id;
    std::string audit_message;
};

// The high-level policy in this chapter's dependency-injection alternative.
// OrderFulfillmentService knows one thing: fulfilling an order is worth an
// audit trail entry. It has no idea whether that entry lands in memory, on
// disk, or in a remote log service, because it depends on AuditSink, the
// abstraction, injected through the constructor, following the same
// manual-DI shape Chapter 4 built for OrderNotificationPolicy. The
// dependency is held as a reference, not a std::unique_ptr: ownership of
// the concrete sink stays with whoever composes the object graph (see
// demo_main.cpp), not with this policy.
class OrderFulfillmentService {
public:
    explicit OrderFulfillmentService(AuditSink& sink) : sink_(sink) {}

    FulfillmentReceipt fulfill_order(const std::string& order_id) const;

#if __cplusplus >= 202302L
    // C++23 refinement: report a sink failure with a typed reason instead
    // of a silent success. Guarded so the mandatory C++20 build path never
    // requires std::expected.
    std::expected<FulfillmentReceipt, std::string> try_fulfill_order(const std::string& order_id) const;
#else
    // C++20 fallback: optional loses the reason for rejection; the caller
    // learns *that* the sink rejected the write, not *why*.
    std::optional<FulfillmentReceipt> try_fulfill_order(const std::string& order_id) const;
#endif

private:
    AuditSink& sink_;
};

}  // namespace shipping::audit

#pragma once

#include <functional>
#include <vector>

#include "billing_domain.hpp"
#include "invoice_rules.hpp"

#if __cplusplus >= 202302L
#include <expected>
#include <string>
#endif

namespace billing {

struct Totals {
    Cents subtotal;
    Cents total;
};

// A non-owning reference to a rule the calculator applies but does not own;
// callers keep the rules alive (the demo and the tests hold them as local
// values or in their own vectors).
using RuleRef = std::reference_wrapper<const InvoiceRule>;

// SRP in effect: InvoiceCalculator's only job is turning an Invoice plus a
// set of rules into totals. It knows nothing about how those totals are
// displayed (InvoiceFormatter) or stored (InMemoryInvoiceRepository), so a
// change to either of those never touches this class, and a change to the
// math here never touches them.
class InvoiceCalculator {
public:
    explicit InvoiceCalculator(std::vector<RuleRef> rules);

    // Every rule receives the invoice's original subtotal, not a running
    // total updated rule by rule, so each rule's adjustment is independent
    // of the order the others ran in.
    Totals totals_for(const Invoice& invoice) const;

#if __cplusplus >= 202302L
    // C++23 refinement: reject an invoice with no line items with a typed
    // reason instead of silently returning a zero total. Guarded so the
    // mandatory C++20 build path never needs std::expected.
    std::expected<Totals, std::string> try_totals_for(const Invoice& invoice) const;
#endif

private:
    std::vector<RuleRef> rules_;
};

}  // namespace billing

#include "invoice_calculator.hpp"

#include <utility>

namespace billing {

InvoiceCalculator::InvoiceCalculator(std::vector<RuleRef> rules) : rules_(std::move(rules)) {}

Totals InvoiceCalculator::totals_for(const Invoice& invoice) const {
    const Cents subtotal = invoice_subtotal(invoice);
    Cents total = subtotal;
    for (const InvoiceRule& rule : rules_) {
        total += rule.adjustment_for(subtotal);
    }
    return Totals{subtotal, total};
}

#if __cplusplus >= 202302L
std::expected<Totals, std::string> InvoiceCalculator::try_totals_for(const Invoice& invoice) const {
    if (invoice.line_items.empty()) {
        return std::unexpected<std::string>("invoice has no line items to total");
    }
    return totals_for(invoice);
}
#endif

}  // namespace billing

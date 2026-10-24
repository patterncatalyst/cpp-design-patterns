#include "billing_domain.hpp"

namespace billing {

Cents invoice_subtotal(const Invoice& invoice) {
    Cents subtotal = 0;
    for (const LineItem& item : invoice.line_items) {
        subtotal += item.unit_price_cents * static_cast<Cents>(item.quantity);
    }
    return subtotal;
}

}  // namespace billing

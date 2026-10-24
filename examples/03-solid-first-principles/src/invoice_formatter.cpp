#include "invoice_formatter.hpp"

#include <sstream>

namespace billing {

namespace {

// Every amount this example ever formats is non-negative: a subtotal, a
// clamped total, or a single line item's extended price. Formatting a
// negative amount is out of scope here on purpose.
std::string format_cents(Cents cents) {
    const auto dollars = cents / 100;
    const auto remainder = cents % 100;
    std::ostringstream out;
    out << "$" << dollars << "." << (remainder < 10 ? "0" : "") << remainder;
    return out.str();
}

}  // namespace

std::string InvoiceFormatter::format(const Invoice& invoice, const Totals& totals) {
    std::ostringstream out;
    out << "Invoice " << invoice.id << " - " << invoice.customer_name << "\n";
    for (const LineItem& item : invoice.line_items) {
        const Cents extended = item.unit_price_cents * static_cast<Cents>(item.quantity);
        out << "  " << item.quantity << " x " << item.description << " @ " << format_cents(item.unit_price_cents)
            << " = " << format_cents(extended) << "\n";
    }
    out << "  Subtotal: " << format_cents(totals.subtotal) << "\n";
    out << "  Total:    " << format_cents(totals.total) << "\n";
    return out.str();
}

}  // namespace billing

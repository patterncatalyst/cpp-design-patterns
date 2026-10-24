#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace billing {

// Money in this domain is tracked as integer cents, never floating point,
// so a total is exact and reproducible in a test assertion.
using Cents = std::int64_t;

// One line on an invoice: a description, a quantity, and a per-unit price.
// LineItem carries data only, no calculation, formatting, or persistence
// behavior of its own, which is what lets the three responsibilities below
// change independently of it and of each other.
struct LineItem {
    std::string description;
    int quantity;
    Cents unit_price_cents;
};

// An invoice to a customer: an id, who it is billed to, and the line items
// that make it up. Like LineItem, Invoice is data, not behavior.
struct Invoice {
    std::string id;
    std::string customer_name;
    std::vector<LineItem> line_items;
};

// The one place an invoice's line items are summed. Every responsibility in
// this example (calculation, formatting, persistence) that needs a subtotal
// calls this function rather than re-deriving it, so an identical invoice
// sums identically no matter which part of the code asked.
Cents invoice_subtotal(const Invoice& invoice);

}  // namespace billing

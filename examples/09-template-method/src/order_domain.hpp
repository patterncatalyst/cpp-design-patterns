#pragma once

#include <string>
#include <vector>

namespace orderexport {

// One line item on an order ready to export: a SKU and the quantity the
// warehouse picked for it.
struct OrderLine {
    std::string sku;
    int quantity;
};

// An order ready to leave the warehouse system as a document: an id, the
// customer it ships to, and the line items a picker already confirmed.
struct Order {
    std::string order_id;
    std::string customer_name;
    std::vector<OrderLine> lines;
};

// Shared by every exporter's trailer step in export_pipeline.cpp, so the
// unit count a trailer reports always agrees with the order's actual
// contents, no matter which format rendered the body above it.
int total_quantity(const Order& order);

}  // namespace orderexport

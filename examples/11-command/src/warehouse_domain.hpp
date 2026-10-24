#pragma once

#include <map>
#include <string>

namespace warehouse {

// A pick line on an order: a quantity of one SKU reserved out of one bin.
// cancelled starts false; a CancelLineCommand flips it and zeroes quantity,
// and its undo can put both back exactly the way they were.
struct OrderLine {
    std::string sku;
    std::string bin;
    int quantity = 0;
    bool cancelled = false;
};

// The receiver every command in this example mutates. WarehouseEditor
// tracks on-hand quantity per (bin, sku) pair and the order lines picked
// against that stock. It knows nothing about commands, undo, or queues;
// it only exposes the small set of operations a command composes its
// execute()/undo() pair out of, the same separation Chapter 8 used to
// keep shipping_domain.cpp's rate math independent of the three Strategy
// expressions that called it.
class WarehouseEditor {
public:
    // Adds delta to the quantity of sku held in bin. delta may be negative,
    // which is what lets every command in this example express its undo as
    // the same operation with the sign flipped, rather than a second,
    // separately-written code path.
    void adjust_stock(const std::string& bin, const std::string& sku, int delta);

    int quantity_of(const std::string& bin, const std::string& sku) const;

    // Test and demo setup: seeds an order line directly, with no stock
    // side effect, the way an earlier picking step would have already
    // deducted that quantity from its bin before this line existed.
    void add_order_line(std::string line_id, OrderLine line);

    OrderLine order_line(const std::string& line_id) const;

    void set_line_state(const std::string& line_id, int quantity, bool cancelled);

private:
    std::map<std::string, std::map<std::string, int>> stock_by_bin_;
    std::map<std::string, OrderLine> order_lines_;
};

}  // namespace warehouse

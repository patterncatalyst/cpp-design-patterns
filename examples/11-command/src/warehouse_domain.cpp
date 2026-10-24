#include "warehouse_domain.hpp"

#include <stdexcept>
#include <utility>

namespace warehouse {

void WarehouseEditor::adjust_stock(const std::string& bin, const std::string& sku, int delta) {
    stock_by_bin_[bin][sku] += delta;
}

int WarehouseEditor::quantity_of(const std::string& bin, const std::string& sku) const {
    const auto bin_it = stock_by_bin_.find(bin);
    if (bin_it == stock_by_bin_.end()) {
        return 0;
    }
    const auto sku_it = bin_it->second.find(sku);
    if (sku_it == bin_it->second.end()) {
        return 0;
    }
    return sku_it->second;
}

void WarehouseEditor::add_order_line(std::string line_id, OrderLine line) {
    order_lines_[std::move(line_id)] = std::move(line);
}

OrderLine WarehouseEditor::order_line(const std::string& line_id) const {
    const auto it = order_lines_.find(line_id);
    if (it == order_lines_.end()) {
        throw std::out_of_range("no order line with id " + line_id);
    }
    return it->second;
}

void WarehouseEditor::set_line_state(const std::string& line_id, int quantity, bool cancelled) {
    auto& line = order_lines_.at(line_id);
    line.quantity = quantity;
    line.cancelled = cancelled;
}

}  // namespace warehouse

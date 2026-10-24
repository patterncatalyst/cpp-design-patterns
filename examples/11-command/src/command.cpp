#include "command.hpp"

#include <utility>

namespace warehouse {

AddStockCommand::AddStockCommand(WarehouseEditor& editor, std::string bin, std::string sku, int quantity)
    : editor_(editor), bin_(std::move(bin)), sku_(std::move(sku)), quantity_(quantity) {}

void AddStockCommand::execute() {
    editor_.adjust_stock(bin_, sku_, quantity_);
}

void AddStockCommand::undo() {
    editor_.adjust_stock(bin_, sku_, -quantity_);
}

MoveItemCommand::MoveItemCommand(WarehouseEditor& editor, std::string sku, std::string from_bin,
                                  std::string to_bin, int quantity)
    : editor_(editor),
      sku_(std::move(sku)),
      from_bin_(std::move(from_bin)),
      to_bin_(std::move(to_bin)),
      quantity_(quantity) {}

void MoveItemCommand::execute() {
    editor_.adjust_stock(from_bin_, sku_, -quantity_);
    editor_.adjust_stock(to_bin_, sku_, quantity_);
}

void MoveItemCommand::undo() {
    editor_.adjust_stock(to_bin_, sku_, -quantity_);
    editor_.adjust_stock(from_bin_, sku_, quantity_);
}

AdjustQuantityCommand::AdjustQuantityCommand(WarehouseEditor& editor, std::string bin, std::string sku,
                                              int new_quantity)
    : editor_(editor), bin_(std::move(bin)), sku_(std::move(sku)), new_quantity_(new_quantity) {}

void AdjustQuantityCommand::execute() {
    previous_quantity_ = editor_.quantity_of(bin_, sku_);
    editor_.adjust_stock(bin_, sku_, new_quantity_ - previous_quantity_);
}

void AdjustQuantityCommand::undo() {
    editor_.adjust_stock(bin_, sku_, previous_quantity_ - new_quantity_);
}

CancelLineCommand::CancelLineCommand(WarehouseEditor& editor, std::string line_id)
    : editor_(editor), line_id_(std::move(line_id)) {}

void CancelLineCommand::execute() {
    previous_state_ = editor_.order_line(line_id_);
    editor_.adjust_stock(previous_state_.bin, previous_state_.sku, previous_state_.quantity);
    editor_.set_line_state(line_id_, 0, /*cancelled=*/true);
}

void CancelLineCommand::undo() {
    editor_.adjust_stock(previous_state_.bin, previous_state_.sku, -previous_state_.quantity);
    editor_.set_line_state(line_id_, previous_state_.quantity, previous_state_.cancelled);
}

}  // namespace warehouse

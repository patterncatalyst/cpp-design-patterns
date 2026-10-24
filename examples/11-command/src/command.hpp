#pragma once

#include <string>

#include "warehouse_domain.hpp"

namespace warehouse {

// The Command interface: every editor operation is an object that knows
// how to run itself (execute) and how to run its own inverse (undo). The
// invoker (CommandHistory, CommandQueue, or a caller holding the pointer
// directly) never knows which concrete operation it is holding; it only
// ever calls these two methods.
class InventoryCommand {
public:
    virtual ~InventoryCommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

// Receives new stock into a bin. Its undo is the same operation with the
// quantity negated: a clean, symmetric inverse that needs no state capture
// at all.
class AddStockCommand : public InventoryCommand {
public:
    AddStockCommand(WarehouseEditor& editor, std::string bin, std::string sku, int quantity);
    void execute() override;
    void undo() override;

private:
    WarehouseEditor& editor_;
    std::string bin_;
    std::string sku_;
    int quantity_;
};

// Moves a quantity of one SKU from one bin to another, e.g. restocking
// from reserve storage to a pick face. Its undo is the same move run
// backward: a second symmetric inverse, this time touching two bins
// instead of one.
class MoveItemCommand : public InventoryCommand {
public:
    MoveItemCommand(WarehouseEditor& editor, std::string sku, std::string from_bin, std::string to_bin,
                     int quantity);
    void execute() override;
    void undo() override;

private:
    WarehouseEditor& editor_;
    std::string sku_;
    std::string from_bin_;
    std::string to_bin_;
    int quantity_;
};

// Overwrites the on-hand quantity of a SKU in a bin to an absolute count,
// the shape a cycle-count correction takes. There is no symmetric inverse
// for "set the quantity to X": undo has to remember what the quantity was
// before this command ran. That capture happens inside execute(), not the
// constructor, because a command built now and queued for later execution
// must read the quantity at the moment it actually runs, not at the
// moment it was constructed.
class AdjustQuantityCommand : public InventoryCommand {
public:
    AdjustQuantityCommand(WarehouseEditor& editor, std::string bin, std::string sku, int new_quantity);
    void execute() override;
    void undo() override;

private:
    WarehouseEditor& editor_;
    std::string bin_;
    std::string sku_;
    int new_quantity_;
    int previous_quantity_ = 0;
};

// Cancels a picked order line, returning its reserved quantity to the bin
// it was picked from. Like AdjustQuantityCommand, there is no arithmetic
// inverse for "cancelled"; undo restores the exact OrderLine snapshot
// captured when execute() ran.
class CancelLineCommand : public InventoryCommand {
public:
    CancelLineCommand(WarehouseEditor& editor, std::string line_id);
    void execute() override;
    void undo() override;

private:
    WarehouseEditor& editor_;
    std::string line_id_;
    OrderLine previous_state_{};
};

}  // namespace warehouse

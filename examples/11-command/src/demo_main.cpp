// Warehouse inventory and order editing, encapsulated as Command objects
// with undo/redo, a FIFO command queue for batched execution, and a
// std::function shortcut for a queued action with nothing to undo.
//
// Build and run via scripts/build.sh from the repo root, or standalone
// from this directory: see README.md "Build, run, observe".

#include <iostream>
#include <memory>

#include "command.hpp"
#include "command_history.hpp"
#include "command_queue.hpp"
#include "restock_log.hpp"
#include "warehouse_domain.hpp"

namespace {

void print_quantity(const char* label, int quantity) {
    std::cout << label << ": " << quantity << '\n';
}

}  // namespace

int main() {
    using namespace warehouse;

    WarehouseEditor editor;

    std::cout << "== Command objects: execute changes state, undo restores it ==\n";
    CommandHistory history;
    history.execute(std::make_unique<AddStockCommand>(editor, "A1", "WIDGET-100", 50));
    print_quantity("A1/WIDGET-100 after AddStockCommand", editor.quantity_of("A1", "WIDGET-100"));

    history.execute(std::make_unique<MoveItemCommand>(editor, "WIDGET-100", "A1", "PICK-FACE-3", 20));
    print_quantity("A1/WIDGET-100 after MoveItemCommand", editor.quantity_of("A1", "WIDGET-100"));
    print_quantity("PICK-FACE-3/WIDGET-100 after MoveItemCommand", editor.quantity_of("PICK-FACE-3", "WIDGET-100"));

    history.undo();
    print_quantity("A1/WIDGET-100 after undo", editor.quantity_of("A1", "WIDGET-100"));
    print_quantity("PICK-FACE-3/WIDGET-100 after undo", editor.quantity_of("PICK-FACE-3", "WIDGET-100"));

    history.redo();
    print_quantity("PICK-FACE-3/WIDGET-100 after redo", editor.quantity_of("PICK-FACE-3", "WIDGET-100"));

    std::cout << "\n== A cycle-count correction and a cancelled order line ==\n";
    editor.add_order_line("LINE-9001", OrderLine{"WIDGET-100", "PICK-FACE-3", 5, /*cancelled=*/false});
    history.execute(std::make_unique<AdjustQuantityCommand>(editor, "PICK-FACE-3", "WIDGET-100", 12));
    print_quantity("PICK-FACE-3/WIDGET-100 after cycle-count correction", editor.quantity_of("PICK-FACE-3", "WIDGET-100"));

    history.execute(std::make_unique<CancelLineCommand>(editor, "LINE-9001"));
    print_quantity("PICK-FACE-3/WIDGET-100 after cancelling LINE-9001", editor.quantity_of("PICK-FACE-3", "WIDGET-100"));
    history.undo();
    print_quantity("PICK-FACE-3/WIDGET-100 after undoing the cancellation", editor.quantity_of("PICK-FACE-3", "WIDGET-100"));

    std::cout << "\n== Command queue: deferred, batched, FIFO execution ==\n";
    CommandQueue queue;
    queue.enqueue(std::make_unique<AddStockCommand>(editor, "B2", "GASKET-7", 30));
    queue.enqueue(std::make_unique<MoveItemCommand>(editor, "GASKET-7", "B2", "PICK-FACE-1", 10));
    queue.enqueue(std::make_unique<AddStockCommand>(editor, "B2", "GASKET-7", 5));
    std::cout << "Pending before processing: " << queue.pending_count() << '\n';
    const auto processed = queue.process_all(&history);
    std::cout << "Processed in submission order: " << processed << '\n';
    print_quantity("B2/GASKET-7 after the batch", editor.quantity_of("B2", "GASKET-7"));
    print_quantity("PICK-FACE-1/GASKET-7 after the batch", editor.quantity_of("PICK-FACE-1", "GASKET-7"));

    std::cout << "\n== std::function shortcut: a queued action with nothing to undo ==\n";
    auto receipt = std::make_unique<RestockReceipt>("GASKET-7");
    const RestockReceipt* receipt_view = receipt.get();
    RestockLogFn log_restock = make_logging_restock(std::move(receipt));
    log_restock(30, "B2");
    log_restock(5, "B2");
    for (const auto& line : receipt_view->lines()) {
        std::cout << line << '\n';
    }

    return 0;
}

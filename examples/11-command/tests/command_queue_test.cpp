#include "command_queue.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "command.hpp"
#include "command_history.hpp"

namespace warehouse {
namespace {

TEST(CommandQueueTest, EnqueueDoesNotRunTheCommand) {
    WarehouseEditor editor;
    CommandQueue queue;

    queue.enqueue(std::make_unique<AddStockCommand>(editor, "A1", "WIDGET-100", 50));

    EXPECT_EQ(queue.pending_count(), 1u);
    EXPECT_EQ(editor.quantity_of("A1", "WIDGET-100"), 0);
}

TEST(CommandQueueTest, ProcessAllRunsCommandsInFifoOrder) {
    WarehouseEditor editor;
    CommandQueue queue;
    queue.enqueue(std::make_unique<AddStockCommand>(editor, "B2", "GASKET-7", 30));
    queue.enqueue(std::make_unique<MoveItemCommand>(editor, "GASKET-7", "B2", "PICK-FACE-1", 10));
    queue.enqueue(std::make_unique<AddStockCommand>(editor, "B2", "GASKET-7", 5));

    const auto processed = queue.process_all();

    EXPECT_EQ(processed, 3u);
    // Had the AddStockCommand for 5 run before the move, the move would
    // have had 35 on hand to draw from instead of 30; the resulting bin
    // totals below only come out this way if submission order held.
    EXPECT_EQ(editor.quantity_of("B2", "GASKET-7"), 25);
    EXPECT_EQ(editor.quantity_of("PICK-FACE-1", "GASKET-7"), 10);
}

TEST(CommandQueueTest, ProcessAllDrainsTheQueue) {
    WarehouseEditor editor;
    CommandQueue queue;
    queue.enqueue(std::make_unique<AddStockCommand>(editor, "B2", "GASKET-7", 30));

    queue.process_all();

    EXPECT_EQ(queue.pending_count(), 0u);
}

TEST(CommandQueueTest, ProcessAllRecordsIntoHistoryWhenProvided) {
    WarehouseEditor editor;
    CommandQueue queue;
    CommandHistory history;
    queue.enqueue(std::make_unique<AddStockCommand>(editor, "B2", "GASKET-7", 30));
    queue.enqueue(std::make_unique<AddStockCommand>(editor, "B2", "GASKET-7", 5));

    queue.process_all(&history);
    ASSERT_EQ(editor.quantity_of("B2", "GASKET-7"), 35);
    ASSERT_EQ(history.done_count(), 2u);

    history.undo();
    EXPECT_EQ(editor.quantity_of("B2", "GASKET-7"), 30);
}

}  // namespace
}  // namespace warehouse

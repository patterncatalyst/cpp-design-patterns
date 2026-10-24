#include "command_history.hpp"

#include <gtest/gtest.h>

#include <memory>

#include "command.hpp"

namespace warehouse {
namespace {

TEST(CommandHistoryTest, ExecuteRunsTheCommandImmediately) {
    WarehouseEditor editor;
    CommandHistory history;

    history.execute(std::make_unique<AddStockCommand>(editor, "A1", "WIDGET-100", 50));

    EXPECT_EQ(editor.quantity_of("A1", "WIDGET-100"), 50);
    EXPECT_EQ(history.done_count(), 1u);
}

TEST(CommandHistoryTest, UndoReturnsFalseWhenThereIsNothingToUndo) {
    CommandHistory history;

    EXPECT_FALSE(history.undo());
}

TEST(CommandHistoryTest, UndoRestoresPriorState) {
    WarehouseEditor editor;
    CommandHistory history;
    history.execute(std::make_unique<AddStockCommand>(editor, "A1", "WIDGET-100", 50));

    const bool undone = history.undo();

    EXPECT_TRUE(undone);
    EXPECT_EQ(editor.quantity_of("A1", "WIDGET-100"), 0);
    EXPECT_EQ(history.done_count(), 0u);
    EXPECT_EQ(history.undone_count(), 1u);
}

TEST(CommandHistoryTest, RedoReappliesAnUndoneCommand) {
    WarehouseEditor editor;
    CommandHistory history;
    history.execute(std::make_unique<AddStockCommand>(editor, "A1", "WIDGET-100", 50));
    history.undo();
    ASSERT_EQ(editor.quantity_of("A1", "WIDGET-100"), 0);

    const bool redone = history.redo();

    EXPECT_TRUE(redone);
    EXPECT_EQ(editor.quantity_of("A1", "WIDGET-100"), 50);
    EXPECT_EQ(history.done_count(), 1u);
    EXPECT_EQ(history.undone_count(), 0u);
}

TEST(CommandHistoryTest, ExecutingANewCommandClearsTheRedoStack) {
    WarehouseEditor editor;
    CommandHistory history;
    history.execute(std::make_unique<AddStockCommand>(editor, "A1", "WIDGET-100", 50));
    history.undo();
    ASSERT_EQ(history.undone_count(), 1u);

    history.execute(std::make_unique<AddStockCommand>(editor, "A1", "WIDGET-100", 10));

    EXPECT_EQ(history.undone_count(), 0u);
    EXPECT_FALSE(history.redo());
    EXPECT_EQ(editor.quantity_of("A1", "WIDGET-100"), 10);
}

TEST(CommandHistoryTest, MixedExecuteUndoRedoSequenceLandsOnTheRightFinalState) {
    WarehouseEditor editor;
    CommandHistory history;

    // Add 50, move 20 to the pick face, undo the move, redo the move, then
    // add 5 more. The final quantities must reflect exactly that sequence.
    history.execute(std::make_unique<AddStockCommand>(editor, "A1", "WIDGET-100", 50));
    history.execute(std::make_unique<MoveItemCommand>(editor, "WIDGET-100", "A1", "PICK-FACE-3", 20));
    ASSERT_EQ(editor.quantity_of("A1", "WIDGET-100"), 30);
    ASSERT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 20);

    ASSERT_TRUE(history.undo());
    ASSERT_EQ(editor.quantity_of("A1", "WIDGET-100"), 50);
    ASSERT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 0);

    ASSERT_TRUE(history.redo());
    ASSERT_EQ(editor.quantity_of("A1", "WIDGET-100"), 30);
    ASSERT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 20);

    history.execute(std::make_unique<AddStockCommand>(editor, "A1", "WIDGET-100", 5));

    EXPECT_EQ(editor.quantity_of("A1", "WIDGET-100"), 35);
    EXPECT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 20);
    EXPECT_EQ(history.done_count(), 3u);
    EXPECT_EQ(history.undone_count(), 0u);
}

}  // namespace
}  // namespace warehouse

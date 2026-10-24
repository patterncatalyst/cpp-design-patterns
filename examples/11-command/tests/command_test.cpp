#include "command.hpp"

#include <gtest/gtest.h>

namespace warehouse {
namespace {

TEST(AddStockCommandTest, ExecuteAddsQuantity) {
    WarehouseEditor editor;
    AddStockCommand command(editor, "A1", "WIDGET-100", 50);

    command.execute();

    EXPECT_EQ(editor.quantity_of("A1", "WIDGET-100"), 50);
}

TEST(AddStockCommandTest, UndoRemovesTheSameQuantity) {
    WarehouseEditor editor;
    AddStockCommand command(editor, "A1", "WIDGET-100", 50);
    command.execute();
    ASSERT_EQ(editor.quantity_of("A1", "WIDGET-100"), 50);

    command.undo();

    EXPECT_EQ(editor.quantity_of("A1", "WIDGET-100"), 0);
}

TEST(MoveItemCommandTest, ExecuteMovesQuantityBetweenBins) {
    WarehouseEditor editor;
    editor.adjust_stock("A1", "WIDGET-100", 50);
    MoveItemCommand command(editor, "WIDGET-100", "A1", "PICK-FACE-3", 20);

    command.execute();

    EXPECT_EQ(editor.quantity_of("A1", "WIDGET-100"), 30);
    EXPECT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 20);
}

TEST(MoveItemCommandTest, UndoMovesItBack) {
    WarehouseEditor editor;
    editor.adjust_stock("A1", "WIDGET-100", 50);
    MoveItemCommand command(editor, "WIDGET-100", "A1", "PICK-FACE-3", 20);
    command.execute();
    ASSERT_EQ(editor.quantity_of("A1", "WIDGET-100"), 30);
    ASSERT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 20);

    command.undo();

    EXPECT_EQ(editor.quantity_of("A1", "WIDGET-100"), 50);
    EXPECT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 0);
}

TEST(AdjustQuantityCommandTest, ExecuteSetsAbsoluteQuantity) {
    WarehouseEditor editor;
    editor.adjust_stock("PICK-FACE-3", "WIDGET-100", 20);
    AdjustQuantityCommand command(editor, "PICK-FACE-3", "WIDGET-100", 12);

    command.execute();

    EXPECT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 12);
}

TEST(AdjustQuantityCommandTest, UndoRestoresThePreviousQuantity) {
    WarehouseEditor editor;
    editor.adjust_stock("PICK-FACE-3", "WIDGET-100", 20);
    AdjustQuantityCommand command(editor, "PICK-FACE-3", "WIDGET-100", 12);
    command.execute();
    ASSERT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 12);

    command.undo();

    EXPECT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 20);
}

TEST(CancelLineCommandTest, ExecuteCancelsLineAndReturnsStock) {
    WarehouseEditor editor;
    editor.adjust_stock("PICK-FACE-3", "WIDGET-100", 12);
    editor.add_order_line("LINE-9001", OrderLine{"WIDGET-100", "PICK-FACE-3", 5, /*cancelled=*/false});
    CancelLineCommand command(editor, "LINE-9001");

    command.execute();

    EXPECT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 17);
    const auto line = editor.order_line("LINE-9001");
    EXPECT_EQ(line.quantity, 0);
    EXPECT_TRUE(line.cancelled);
}

TEST(CancelLineCommandTest, UndoRestoresLineAndReReservesStock) {
    WarehouseEditor editor;
    editor.adjust_stock("PICK-FACE-3", "WIDGET-100", 12);
    editor.add_order_line("LINE-9001", OrderLine{"WIDGET-100", "PICK-FACE-3", 5, /*cancelled=*/false});
    CancelLineCommand command(editor, "LINE-9001");
    command.execute();
    ASSERT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 17);

    command.undo();

    EXPECT_EQ(editor.quantity_of("PICK-FACE-3", "WIDGET-100"), 12);
    const auto line = editor.order_line("LINE-9001");
    EXPECT_EQ(line.quantity, 5);
    EXPECT_FALSE(line.cancelled);
}

}  // namespace
}  // namespace warehouse

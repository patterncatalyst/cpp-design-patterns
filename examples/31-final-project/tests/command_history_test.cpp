#include "command_history.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "shipment_command.hpp"

namespace finalproject31 {
namespace {

using ::testing::InSequence;

class MockShipmentCommand : public ShipmentCommand {
public:
    MOCK_METHOD(void, execute, (), (override));
    MOCK_METHOD(void, undo, (), (override));
    MOCK_METHOD(std::string, description, (), (const, override));
};

TEST(CommandHistoryTest, ExecuteRunsTheCommandImmediately) {
    auto command = std::make_unique<MockShipmentCommand>();
    EXPECT_CALL(*command, execute()).Times(1);

    CommandHistory history;
    history.execute(std::move(command));

    EXPECT_EQ(history.executed_count(), 1u);
}

TEST(CommandHistoryTest, UndoLastReversesTheMostRecentlyExecutedCommandOnly) {
    auto first = std::make_unique<MockShipmentCommand>();
    auto second = std::make_unique<MockShipmentCommand>();
    MockShipmentCommand* first_ptr = first.get();
    MockShipmentCommand* second_ptr = second.get();

    EXPECT_CALL(*first, execute()).Times(1);
    EXPECT_CALL(*second, execute()).Times(1);
    EXPECT_CALL(*second, undo()).Times(1);
    EXPECT_CALL(*first, undo()).Times(0);

    CommandHistory history;
    history.execute(std::move(first));
    history.execute(std::move(second));

    EXPECT_TRUE(history.undo_last());

    // Suppress unused-variable warnings; the raw pointers exist only to
    // attach expectations above, both commands are still owned by history.
    (void)first_ptr;
    (void)second_ptr;
}

TEST(CommandHistoryTest, UndoLastIsLastInFirstOut) {
    auto first = std::make_unique<MockShipmentCommand>();
    auto second = std::make_unique<MockShipmentCommand>();

    InSequence sequence;
    EXPECT_CALL(*first, execute()).Times(1);
    EXPECT_CALL(*second, execute()).Times(1);
    EXPECT_CALL(*second, undo()).Times(1);
    EXPECT_CALL(*first, undo()).Times(1);

    CommandHistory history;
    history.execute(std::move(first));
    history.execute(std::move(second));

    EXPECT_TRUE(history.undo_last());
    EXPECT_TRUE(history.undo_last());
}

TEST(CommandHistoryTest, UndoLastOnAnEmptyHistoryReturnsFalse) {
    CommandHistory history;

    EXPECT_FALSE(history.undo_last());
}

}  // namespace
}  // namespace finalproject31

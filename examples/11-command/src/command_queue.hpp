#pragma once

#include <cstddef>
#include <deque>
#include <memory>

#include "command.hpp"
#include "command_history.hpp"

namespace warehouse {

// A FIFO queue of commands submitted for deferred, batched execution.
// Floor staff can queue stock moves and adjustments throughout a shift;
// enqueue() never runs anything, it only records submission order.
// process_all() is the batch step, usually run once at the end of the
// shift, that drains the queue and runs every command in the exact order
// it was submitted.
class CommandQueue {
public:
    void enqueue(std::unique_ptr<InventoryCommand> command);
    std::size_t pending_count() const;

    // Runs every queued command in FIFO order. When history is non-null,
    // each command is run through history->execute() instead of calling
    // execute() directly, so the whole batch remains individually
    // undoable afterward; pass nullptr when the batch is fire-and-forget.
    // Returns the number of commands processed.
    std::size_t process_all(CommandHistory* history = nullptr);

private:
    std::deque<std::unique_ptr<InventoryCommand>> queue_;
};

}  // namespace warehouse

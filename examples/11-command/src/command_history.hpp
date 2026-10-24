#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "command.hpp"

namespace warehouse {

// Owns the undo and redo stacks for a sequence of InventoryCommands.
// execute() runs a command immediately and pushes it onto the done stack.
// undo() pops the most recently done command, runs its inverse, and moves
// it onto the undone stack. redo() does the reverse: pop the most recently
// undone command, run it forward again, and move it back onto the done
// stack. Running a new command through execute() after an undo clears the
// redo stack, the same rule a text editor's undo history follows: once a
// new edit happens, the branch of history you undid away from is gone.
class CommandHistory {
public:
    void execute(std::unique_ptr<InventoryCommand> command);
    bool undo();
    bool redo();

    std::size_t done_count() const;
    std::size_t undone_count() const;

private:
    std::vector<std::unique_ptr<InventoryCommand>> done_;
    std::vector<std::unique_ptr<InventoryCommand>> undone_;
};

}  // namespace warehouse

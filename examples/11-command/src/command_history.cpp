#include "command_history.hpp"

#include <utility>

namespace warehouse {

void CommandHistory::execute(std::unique_ptr<InventoryCommand> command) {
    command->execute();
    undone_.clear();
    done_.push_back(std::move(command));
}

bool CommandHistory::undo() {
    if (done_.empty()) {
        return false;
    }
    auto command = std::move(done_.back());
    done_.pop_back();
    command->undo();
    undone_.push_back(std::move(command));
    return true;
}

bool CommandHistory::redo() {
    if (undone_.empty()) {
        return false;
    }
    auto command = std::move(undone_.back());
    undone_.pop_back();
    command->execute();
    done_.push_back(std::move(command));
    return true;
}

std::size_t CommandHistory::done_count() const {
    return done_.size();
}

std::size_t CommandHistory::undone_count() const {
    return undone_.size();
}

}  // namespace warehouse

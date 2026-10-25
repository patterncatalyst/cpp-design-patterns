#include "command_history.hpp"

#include <utility>

namespace finalproject31 {

void CommandHistory::execute(std::unique_ptr<ShipmentCommand> command) {
    command->execute();
    done_.push_back(std::move(command));
}

bool CommandHistory::undo_last() {
    if (done_.empty()) {
        return false;
    }
    std::unique_ptr<ShipmentCommand> command = std::move(done_.back());
    done_.pop_back();
    command->undo();
    return true;
}

}  // namespace finalproject31

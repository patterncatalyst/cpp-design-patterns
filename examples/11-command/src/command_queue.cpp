#include "command_queue.hpp"

#include <utility>

namespace warehouse {

void CommandQueue::enqueue(std::unique_ptr<InventoryCommand> command) {
    queue_.push_back(std::move(command));
}

std::size_t CommandQueue::pending_count() const {
    return queue_.size();
}

std::size_t CommandQueue::process_all(CommandHistory* history) {
    std::size_t processed = 0;
    while (!queue_.empty()) {
        auto command = std::move(queue_.front());
        queue_.pop_front();
        if (history != nullptr) {
            history->execute(std::move(command));
        } else {
            command->execute();
        }
        ++processed;
    }
    return processed;
}

}  // namespace warehouse

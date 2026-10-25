#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "shipment_command.hpp"

namespace finalproject31 {

// A minimal undo stack: execute() runs a command immediately and remembers
// it; undo_last() reverses the most recently executed command that has
// not already been undone, last in, first out. This is the receiver every
// other component in this example's demo and tests drives commands
// through, rather than calling execute() directly and losing the command
// the moment the call returns.
class CommandHistory {
public:
    void execute(std::unique_ptr<ShipmentCommand> command);
    bool undo_last();

    std::size_t executed_count() const { return done_.size(); }

private:
    std::vector<std::unique_ptr<ShipmentCommand>> done_;
};

}  // namespace finalproject31

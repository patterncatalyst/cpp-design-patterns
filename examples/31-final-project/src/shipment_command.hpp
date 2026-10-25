#pragma once

#include <string>

namespace finalproject31 {

// Command: an operation on a shipment becomes an object with its own
// execute() and undo(), rather than a bare call from whatever triggered
// it. Once a request is an object, CommandHistory can hold onto it long
// enough to reverse it, something a function call that already returned
// cannot do.
class ShipmentCommand {
public:
    virtual ~ShipmentCommand() = default;

    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string description() const = 0;
};

}  // namespace finalproject31

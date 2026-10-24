#pragma once

#include <memory>

#include "shipment_domain.hpp"

namespace shipment {

// Expression 1: the GoF State pattern. Each ShipmentStatus is its own
// type; which events are legal and what status comes next live in that
// type's method overrides, not in a switch statement the context has to
// maintain. The default implementation of every event rejects it, so a
// concrete state only has to override the handful of events it actually
// accepts.
class ShipmentState {
public:
    virtual ~ShipmentState() = default;
    virtual ShipmentStatus status() const = 0;

    virtual std::unique_ptr<ShipmentState> pay() const { reject(ShipmentEvent::Pay); }
    virtual std::unique_ptr<ShipmentState> ship() const { reject(ShipmentEvent::Ship); }
    virtual std::unique_ptr<ShipmentState> deliver() const { reject(ShipmentEvent::Deliver); }
    virtual std::unique_ptr<ShipmentState> cancel() const { reject(ShipmentEvent::Cancel); }

protected:
    [[noreturn]] void reject(ShipmentEvent event) const;
};

std::unique_ptr<ShipmentState> make_created_state();

// GofStateMachine is the context object: it owns exactly one ShipmentState
// at a time and forwards every event to it, replacing itself with
// whatever state comes back. An illegal event throws IllegalTransition
// from inside the current state's override before the context's own
// pointer is ever reassigned, so a rejected event leaves the machine in
// its original status.
class GofStateMachine {
public:
    GofStateMachine();

    ShipmentStatus status() const { return state_->status(); }

    void pay() { state_ = state_->pay(); }
    void ship() { state_ = state_->ship(); }
    void deliver() { state_ = state_->deliver(); }
    void cancel() { state_ = state_->cancel(); }

private:
    std::unique_ptr<ShipmentState> state_;
};

}  // namespace shipment

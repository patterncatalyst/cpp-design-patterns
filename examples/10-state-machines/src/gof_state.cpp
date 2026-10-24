#include "gof_state.hpp"

namespace shipment {

void ShipmentState::reject(ShipmentEvent event) const {
    throw IllegalTransition(status(), event);
}

namespace {

class DeliveredState : public ShipmentState {
public:
    ShipmentStatus status() const override { return ShipmentStatus::Delivered; }
};

class CancelledState : public ShipmentState {
public:
    ShipmentStatus status() const override { return ShipmentStatus::Cancelled; }
};

class InTransitState : public ShipmentState {
public:
    ShipmentStatus status() const override { return ShipmentStatus::InTransit; }

    std::unique_ptr<ShipmentState> deliver() const override { return std::make_unique<DeliveredState>(); }
};

class PaidState : public ShipmentState {
public:
    ShipmentStatus status() const override { return ShipmentStatus::Paid; }

    std::unique_ptr<ShipmentState> ship() const override { return std::make_unique<InTransitState>(); }
    std::unique_ptr<ShipmentState> cancel() const override { return std::make_unique<CancelledState>(); }
};

class CreatedState : public ShipmentState {
public:
    ShipmentStatus status() const override { return ShipmentStatus::Created; }

    std::unique_ptr<ShipmentState> pay() const override { return std::make_unique<PaidState>(); }
    std::unique_ptr<ShipmentState> cancel() const override { return std::make_unique<CancelledState>(); }
};

}  // namespace

std::unique_ptr<ShipmentState> make_created_state() {
    return std::make_unique<CreatedState>();
}

GofStateMachine::GofStateMachine() : state_(make_created_state()) {}

}  // namespace shipment

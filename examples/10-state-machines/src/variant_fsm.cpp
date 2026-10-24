#include "variant_fsm.hpp"

namespace shipment {

namespace {

// The standard "overloaded lambda set" helper: a type that inherits the
// call operator of every lambda passed to it, so std::visit can be given
// one callable per alternative instead of one callable that switches on
// every alternative itself.
template <typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <typename... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

}  // namespace

ShipmentStatus status_of(const ShipmentVariant& state) {
    return std::visit(
        Overloaded{
            [](CreatedTag) { return ShipmentStatus::Created; },
            [](PaidTag) { return ShipmentStatus::Paid; },
            [](InTransitTag) { return ShipmentStatus::InTransit; },
            [](DeliveredTag) { return ShipmentStatus::Delivered; },
            [](CancelledTag) { return ShipmentStatus::Cancelled; },
        },
        state);
}

ShipmentVariant apply_event(const ShipmentVariant& state, ShipmentEvent event) {
    return std::visit(
        Overloaded{
            [event](CreatedTag) -> ShipmentVariant {
                if (event == ShipmentEvent::Pay) return PaidTag{};
                if (event == ShipmentEvent::Cancel) return CancelledTag{};
                throw IllegalTransition(ShipmentStatus::Created, event);
            },
            [event](PaidTag) -> ShipmentVariant {
                if (event == ShipmentEvent::Ship) return InTransitTag{};
                if (event == ShipmentEvent::Cancel) return CancelledTag{};
                throw IllegalTransition(ShipmentStatus::Paid, event);
            },
            [event](InTransitTag) -> ShipmentVariant {
                if (event == ShipmentEvent::Deliver) return DeliveredTag{};
                throw IllegalTransition(ShipmentStatus::InTransit, event);
            },
            [event](DeliveredTag) -> ShipmentVariant { throw IllegalTransition(ShipmentStatus::Delivered, event); },
            [event](CancelledTag) -> ShipmentVariant { throw IllegalTransition(ShipmentStatus::Cancelled, event); },
        },
        state);
}

}  // namespace shipment

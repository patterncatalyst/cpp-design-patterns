#include "dispatch_map.hpp"

#include "message_handlers.hpp"

namespace dispatch25 {

namespace {

void handle_unrecognized_wire_tag(const CarrierWireMessage& message, TrackingSink& sink) {
    sink.record(message.tracking_id, "unrecognized wire tag \"" + message.wire_tag + "\", raw message ignored");
}

// Adapts a closed-set handler, written against CarrierMessage, so it can
// seed the open map's CarrierWireMessage-keyed table. Every built-in wire
// tag below resolves to the same handler functions the jump table and the
// constexpr table call; only the key that selects them is open-ended.
template <typename Handler>
MessageDispatchMap::Handler adapt(CarrierMessageType type, Handler handler) {
    return [type, handler](const CarrierWireMessage& wire, TrackingSink& sink) {
        handler(CarrierMessage{type, wire.tracking_id, wire.detail}, sink);
    };
}

}  // namespace

MessageDispatchMap::MessageDispatchMap() {
    handlers_.emplace("CREATED", adapt(CarrierMessageType::Created, &handle_created));
    handlers_.emplace("PICKED_UP", adapt(CarrierMessageType::PickedUp, &handle_picked_up));
    handlers_.emplace("IN_TRANSIT", adapt(CarrierMessageType::InTransit, &handle_in_transit));
    handlers_.emplace("OUT_FOR_DELIVERY", adapt(CarrierMessageType::OutForDelivery, &handle_out_for_delivery));
    handlers_.emplace("DELIVERED", adapt(CarrierMessageType::Delivered, &handle_delivered));
    handlers_.emplace("DELIVERY_EXCEPTION", adapt(CarrierMessageType::DeliveryException, &handle_delivery_exception));
}

void MessageDispatchMap::register_handler(std::string wire_tag, Handler handler) {
    handlers_[std::move(wire_tag)] = std::move(handler);
}

void MessageDispatchMap::dispatch(const CarrierWireMessage& message, TrackingSink& sink) const {
    const auto it = handlers_.find(message.wire_tag);
    if (it == handlers_.end()) {
        handle_unrecognized_wire_tag(message, sink);
        return;
    }
    it->second(message, sink);
}

#if __cplusplus >= 202302L

FlatMessageDispatchMap::FlatMessageDispatchMap() {
    handlers_.emplace("CREATED", adapt(CarrierMessageType::Created, &handle_created));
    handlers_.emplace("PICKED_UP", adapt(CarrierMessageType::PickedUp, &handle_picked_up));
    handlers_.emplace("IN_TRANSIT", adapt(CarrierMessageType::InTransit, &handle_in_transit));
    handlers_.emplace("OUT_FOR_DELIVERY", adapt(CarrierMessageType::OutForDelivery, &handle_out_for_delivery));
    handlers_.emplace("DELIVERED", adapt(CarrierMessageType::Delivered, &handle_delivered));
    handlers_.emplace("DELIVERY_EXCEPTION", adapt(CarrierMessageType::DeliveryException, &handle_delivery_exception));
}

void FlatMessageDispatchMap::register_handler(std::string wire_tag, Handler handler) {
    handlers_[std::move(wire_tag)] = std::move(handler);
}

void FlatMessageDispatchMap::dispatch(const CarrierWireMessage& message, TrackingSink& sink) const {
    const auto it = handlers_.find(message.wire_tag);
    if (it == handlers_.end()) {
        handle_unrecognized_wire_tag(message, sink);
        return;
    }
    it->second(message, sink);
}

#endif

}  // namespace dispatch25

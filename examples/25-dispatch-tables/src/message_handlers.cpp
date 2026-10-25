#include "message_handlers.hpp"

namespace dispatch25 {

void handle_created(const CarrierMessage& message, TrackingSink& sink) {
    sink.record(message.tracking_id, "created: " + message.detail);
}

void handle_picked_up(const CarrierMessage& message, TrackingSink& sink) {
    sink.record(message.tracking_id, "picked-up: " + message.detail);
}

void handle_in_transit(const CarrierMessage& message, TrackingSink& sink) {
    sink.record(message.tracking_id, "in-transit: " + message.detail);
}

void handle_out_for_delivery(const CarrierMessage& message, TrackingSink& sink) {
    sink.record(message.tracking_id, "out-for-delivery: " + message.detail);
}

void handle_delivered(const CarrierMessage& message, TrackingSink& sink) {
    sink.record(message.tracking_id, "delivered: " + message.detail);
}

void handle_delivery_exception(const CarrierMessage& message, TrackingSink& sink) {
    sink.record(message.tracking_id, "delivery-exception: " + message.detail);
}

void handle_unrecognized_type(const CarrierMessage& message, TrackingSink& sink) {
    sink.record(message.tracking_id, "unrecognized message type, raw value ignored");
}

}  // namespace dispatch25

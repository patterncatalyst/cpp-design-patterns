#include "notification.hpp"

#include <utility>

namespace shipping {

std::string EmailTransport::deliver(const std::string& recipient, const std::string& message) const {
    return "EMAIL to " + recipient + ": " + message;
}

std::string SmsTransport::deliver(const std::string& recipient, const std::string& message) const {
    return "SMS to " + recipient + ": " + message;
}

Notification::Notification(std::shared_ptr<NotificationTransport> transport) : transport_(std::move(transport)) {}

std::string Notification::send(const std::string& recipient) const {
    return transport_->deliver(recipient, format_message());
}

ShipmentCreatedNotification::ShipmentCreatedNotification(std::shared_ptr<NotificationTransport> transport,
                                                           std::string tracking_number)
    : Notification(std::move(transport)), tracking_number_(std::move(tracking_number)) {}

std::string ShipmentCreatedNotification::format_message() const {
    return "Your shipment " + tracking_number_ + " has been created.";
}

DeliveryDelayedNotification::DeliveryDelayedNotification(std::shared_ptr<NotificationTransport> transport,
                                                           std::string tracking_number, int delay_hours)
    : Notification(std::move(transport)), tracking_number_(std::move(tracking_number)), delay_hours_(delay_hours) {}

std::string DeliveryDelayedNotification::format_message() const {
    return "Shipment " + tracking_number_ + " is delayed by " + std::to_string(delay_hours_) + " hours.";
}

}  // namespace shipping

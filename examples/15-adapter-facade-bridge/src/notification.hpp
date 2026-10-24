#pragma once

#include <memory>
#include <string>

namespace shipping {

// Bridge implementation axis: how a message actually gets delivered. Varies
// independently of which kind of notification is being sent.
class NotificationTransport {
public:
    virtual ~NotificationTransport() = default;
    virtual std::string deliver(const std::string& recipient, const std::string& message) const = 0;
};

class EmailTransport : public NotificationTransport {
public:
    std::string deliver(const std::string& recipient, const std::string& message) const override;
};

class SmsTransport : public NotificationTransport {
public:
    std::string deliver(const std::string& recipient, const std::string& message) const override;
};

// Bridge abstraction axis: the kind of event being communicated. Holds a
// NotificationTransport by pointer, not by inheriting from it, so a new
// refined Notification never needs one subclass per transport and a new
// transport never needs one subclass per notification kind.
class Notification {
public:
    explicit Notification(std::shared_ptr<NotificationTransport> transport);
    virtual ~Notification() = default;

    std::string send(const std::string& recipient) const;

protected:
    virtual std::string format_message() const = 0;

private:
    std::shared_ptr<NotificationTransport> transport_;
};

class ShipmentCreatedNotification : public Notification {
public:
    ShipmentCreatedNotification(std::shared_ptr<NotificationTransport> transport, std::string tracking_number);

protected:
    std::string format_message() const override;

private:
    std::string tracking_number_;
};

class DeliveryDelayedNotification : public Notification {
public:
    DeliveryDelayedNotification(std::shared_ptr<NotificationTransport> transport, std::string tracking_number,
                                 int delay_hours);

protected:
    std::string format_message() const override;

private:
    std::string tracking_number_;
    int delay_hours_;
};

}  // namespace shipping

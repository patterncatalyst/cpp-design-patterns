#pragma once

#include <string>
#include <vector>

namespace shipping {

// The set of events a warehouse, carrier, or customer-notification
// component can raise through the mediator. Only DispatchCoordinator below
// decides which collaborator responds to which event; no colleague class
// here knows that any other colleague even exists.
enum class DispatchEvent {
    OrderReadyForPickup,
    PickedUpByCarrier,
    OutForDelivery,
    DeliveryException,
};

class DispatchColleague;

// The Mediator interface. Every colleague raises an event by calling
// notify() on whatever DispatchMediator it is bound to; it never calls
// another colleague's method directly.
class DispatchMediator {
public:
    virtual ~DispatchMediator() = default;
    virtual void notify(DispatchColleague& sender, DispatchEvent event, const std::string& order_id) = 0;
};

// Shared base for every component that talks through the mediator. A
// colleague is constructed with no knowledge of any other colleague and no
// mediator at all; DispatchCoordinator's constructor is the only place
// that wires a colleague to a mediator, through the private bind() it
// reaches as a friend. Note what this class has no member for: a pointer
// to WarehouseStation, CarrierStation, or CustomerNotificationStation. The
// only thing a colleague can reach is its own mediator.
class DispatchColleague {
public:
    virtual ~DispatchColleague() = default;

protected:
    void raise(DispatchEvent event, const std::string& order_id);

private:
    friend class DispatchCoordinator;
    void bind(DispatchMediator& mediator) { mediator_ = &mediator; }

    DispatchMediator* mediator_ = nullptr;
};

class WarehouseStation : public DispatchColleague {
public:
    // Called when a packed order is staged and ready for a carrier pickup.
    void mark_order_ready(const std::string& order_id);
};

class CarrierStation : public DispatchColleague {
public:
    void confirm_pickup(const std::string& order_id);
    void mark_out_for_delivery(const std::string& order_id);
    void report_exception(const std::string& order_id, const std::string& reason);

    const std::vector<std::string>& picked_up_orders() const { return picked_up_orders_; }
    const std::vector<std::string>& exception_reasons() const { return exception_reasons_; }

private:
    std::vector<std::string> picked_up_orders_;
    std::vector<std::string> exception_reasons_;
};

class CustomerNotificationStation : public DispatchColleague {
public:
    void send(const std::string& order_id, const std::string& message);
    const std::vector<std::string>& sent_messages() const { return sent_messages_; }

private:
    std::vector<std::string> sent_messages_;
};

// The Mediator. It owns no order state of its own; it only knows the three
// colleagues and which one responds to which event, which is exactly the
// many-to-many knowledge this chapter moves out of the colleagues
// themselves and into one place.
class DispatchCoordinator : public DispatchMediator {
public:
    DispatchCoordinator(WarehouseStation& warehouse, CarrierStation& carrier,
                         CustomerNotificationStation& notifier);

    void notify(DispatchColleague& sender, DispatchEvent event, const std::string& order_id) override;

private:
    WarehouseStation* warehouse_;
    CarrierStation* carrier_;
    CustomerNotificationStation* notifier_;
};

}  // namespace shipping

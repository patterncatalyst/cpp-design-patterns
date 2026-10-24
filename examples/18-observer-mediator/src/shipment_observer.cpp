#include "shipment_observer.hpp"

#include <stdexcept>
#include <utility>

namespace shipping {
namespace {

bool is_terminal(ShipmentStatus status) {
    return status == ShipmentStatus::Delivered || status == ShipmentStatus::Exception;
}

}  // namespace

const char* to_string(ShipmentStatus status) {
    switch (status) {
        case ShipmentStatus::Created:
            return "Created";
        case ShipmentStatus::PickedUp:
            return "PickedUp";
        case ShipmentStatus::InTransit:
            return "InTransit";
        case ShipmentStatus::OutForDelivery:
            return "OutForDelivery";
        case ShipmentStatus::Delivered:
            return "Delivered";
        case ShipmentStatus::Exception:
            return "Exception";
    }
    return "Unknown";
}

ScopedSubscription::ScopedSubscription(ShipmentTracker& tracker, std::size_t id) : tracker_(&tracker), id_(id) {}

ScopedSubscription::~ScopedSubscription() {
    release();
}

ScopedSubscription::ScopedSubscription(ScopedSubscription&& other) noexcept
    : tracker_(other.tracker_), id_(other.id_) {
    other.tracker_ = nullptr;
}

ScopedSubscription& ScopedSubscription::operator=(ScopedSubscription&& other) noexcept {
    if (this != &other) {
        release();
        tracker_ = other.tracker_;
        id_ = other.id_;
        other.tracker_ = nullptr;
    }
    return *this;
}

void ScopedSubscription::release() {
    if (tracker_ != nullptr) {
        tracker_->unsubscribe(id_);
        tracker_ = nullptr;
    }
}

ShipmentTracker::ShipmentTracker(std::string tracking_id) : tracking_id_(std::move(tracking_id)) {}

ScopedSubscription ShipmentTracker::subscribe(ShipmentObserver& observer) {
    return subscribe([&observer](const std::string& tracking_id, ShipmentStatus old_status,
                                  ShipmentStatus new_status) {
        observer.on_status_changed(tracking_id, old_status, new_status);
    });
}

ScopedSubscription ShipmentTracker::subscribe(StatusChangeSlot slot) {
    if (is_terminal(status_)) {
        throw std::logic_error("cannot subscribe to a shipment that already reached a terminal status");
    }
    const std::size_t id = next_id_++;
    slots_.emplace_back(id, std::move(slot));
    return ScopedSubscription(*this, id);
}

#if __cplusplus >= 202302L
std::expected<ScopedSubscription, std::string> ShipmentTracker::try_subscribe(StatusChangeSlot slot) {
    if (is_terminal(status_)) {
        return std::unexpected<std::string>("cannot subscribe to a shipment that already reached a terminal status");
    }
    const std::size_t id = next_id_++;
    slots_.emplace_back(id, std::move(slot));
    return ScopedSubscription(*this, id);
}
#endif

void ShipmentTracker::unsubscribe(std::size_t id) {
    for (auto it = slots_.begin(); it != slots_.end(); ++it) {
        if (it->first == id) {
            slots_.erase(it);
            return;
        }
    }
}

void ShipmentTracker::advance_status(ShipmentStatus new_status) {
    if (new_status == status_) {
        return;
    }
    const ShipmentStatus old_status = status_;
    status_ = new_status;
    // Copy the slot list before notifying: a slot is free to unsubscribe
    // itself, or another slot, during the callback, including by letting a
    // ScopedSubscription go out of scope, and iterating a snapshot keeps
    // that from invalidating slots_ mid-notify.
    const auto snapshot = slots_;
    for (const auto& [id, slot] : snapshot) {
        slot(tracking_id_, old_status, new_status);
    }
}

void DashboardView::on_status_changed(const std::string& tracking_id, ShipmentStatus /*old_status*/,
                                       ShipmentStatus new_status) {
    current_status_line_ = tracking_id + ": " + to_string(new_status);
}

void AlertingService::on_status_changed(const std::string& tracking_id, ShipmentStatus old_status,
                                         ShipmentStatus new_status) {
    if (new_status == ShipmentStatus::Exception) {
        alerts_.push_back(tracking_id + " flagged: " + to_string(old_status) + " -> " + to_string(new_status));
    }
}

}  // namespace shipping

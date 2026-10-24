#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#if __cplusplus >= 202302L
#include <expected>
#endif

namespace shipping {

// A shipment's lifecycle, from creation at the warehouse to the final
// delivered (or exception) state. ShipmentTracker is the Observer subject:
// every status change fans out to whatever is currently subscribed.
enum class ShipmentStatus {
    Created,
    PickedUp,
    InTransit,
    OutForDelivery,
    Delivered,
    Exception,
};

const char* to_string(ShipmentStatus status);

// The callback every observer of a ShipmentTracker is, underneath: a plain
// function signature, not an interface. A caller with nothing but a lambda
// (the audit log in this example) can subscribe without writing a class at
// all; the interface-based ShipmentObserver further down is sugar built on
// top of exactly this slot.
using StatusChangeSlot =
    std::function<void(const std::string& tracking_id, ShipmentStatus old_status, ShipmentStatus new_status)>;

class ShipmentTracker;

// RAII handle returned by every subscribe call. Holding this object open is
// what keeps a subscription alive; letting it go out of scope (or calling
// release() early) unsubscribes automatically. This is the mechanism that
// keeps a destroyed observer from ever being notified again: an observer
// that owns its ScopedSubscription as a later-declared member removes
// itself from the tracker's list during its own destruction, before any
// later advance_status() call could reach freed memory.
class ScopedSubscription {
public:
    ScopedSubscription() = default;
    ScopedSubscription(ShipmentTracker& tracker, std::size_t id);
    ~ScopedSubscription();

    ScopedSubscription(const ScopedSubscription&) = delete;
    ScopedSubscription& operator=(const ScopedSubscription&) = delete;

    ScopedSubscription(ScopedSubscription&& other) noexcept;
    ScopedSubscription& operator=(ScopedSubscription&& other) noexcept;

    // Unsubscribe now instead of waiting for destruction. Safe to call
    // more than once, or on a default-constructed (empty) handle.
    void release();

private:
    ShipmentTracker* tracker_ = nullptr;
    std::size_t id_ = 0;
};

// Classic Gang of Four shape: an interface a subject notifies through a
// reference, used when an observer is a named, reusable collaborator with
// its own identity, not a one-off callback. DashboardView and
// AlertingService below both implement this interface.
class ShipmentObserver {
public:
    virtual ~ShipmentObserver() = default;
    virtual void on_status_changed(const std::string& tracking_id, ShipmentStatus old_status,
                                    ShipmentStatus new_status) = 0;
};

// The Observer subject. Holds the shipment's current status and a list of
// subscribed slots; advancing the status notifies every slot still
// subscribed, in subscription order, then returns.
class ShipmentTracker {
public:
    explicit ShipmentTracker(std::string tracking_id);

    const std::string& tracking_id() const { return tracking_id_; }
    ShipmentStatus status() const { return status_; }
    std::size_t subscriber_count() const { return slots_.size(); }

    // Subscribe an interface-based observer. Internally this is sugar over
    // subscribe(StatusChangeSlot): the observer's on_status_changed is
    // wrapped in a slot that forwards to it.
    //
    // C++20 contract: throws std::logic_error if the shipment has already
    // reached a terminal status (Delivered or Exception), since a new
    // subscriber there would never receive a single notification.
    ScopedSubscription subscribe(ShipmentObserver& observer);

    // Subscribe a plain callable: the lightweight form, no interface or
    // named class required. Same terminal-status contract as above.
    ScopedSubscription subscribe(StatusChangeSlot slot);

#if __cplusplus >= 202302L
    // C++23 refinement: the same terminal-status rejection, reported as a
    // typed error instead of an exception. Guarded so the mandatory C++20
    // build path never needs std::expected.
    std::expected<ScopedSubscription, std::string> try_subscribe(StatusChangeSlot slot);
#endif

    // Move the shipment to a new status and notify every current
    // subscriber with the old and new status. No-op (no notification) if
    // the status is unchanged.
    void advance_status(ShipmentStatus new_status);

private:
    friend class ScopedSubscription;
    void unsubscribe(std::size_t id);

    std::string tracking_id_;
    ShipmentStatus status_ = ShipmentStatus::Created;
    std::vector<std::pair<std::size_t, StatusChangeSlot>> slots_;
    std::size_t next_id_ = 0;
};

// Concrete observer: keeps the most recent status as one display line. A
// real dashboard would push this to a UI; this example keeps it readable
// in a test assertion.
class DashboardView : public ShipmentObserver {
public:
    void on_status_changed(const std::string& tracking_id, ShipmentStatus old_status,
                            ShipmentStatus new_status) override;

    const std::string& current_status_line() const { return current_status_line_; }

private:
    std::string current_status_line_;
};

// Concrete observer: raises an alert only when a shipment's status becomes
// Exception, recording the transition that triggered it.
class AlertingService : public ShipmentObserver {
public:
    void on_status_changed(const std::string& tracking_id, ShipmentStatus old_status,
                            ShipmentStatus new_status) override;

    const std::vector<std::string>& alerts() const { return alerts_; }

private:
    std::vector<std::string> alerts_;
};

}  // namespace shipping

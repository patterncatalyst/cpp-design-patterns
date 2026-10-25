#pragma once

#include <string>
#include <vector>

#include "lifecycle_observer.hpp"
#include "shipment_types.hpp"

namespace finalproject31 {

// Shipment is both the State component's context, it owns the current
// ShipmentStatus and enforces which events apply to it, and the Observer
// component's subject, it notifies every subscribed LifecycleObserver the
// instant a transition succeeds. Keeping both roles on one type is what
// lets a single call to advance() demonstrate State and Observer meeting
// at a seam: the state machine's own success condition is the observer's
// trigger condition, with nothing in between.
class Shipment {
public:
    Shipment(std::string id, std::string carrier_name, std::string destination_zone, double weight_kg,
              Cents quoted_rate_cents);

    const std::string& id() const { return id_; }
    const std::string& carrier_name() const { return carrier_name_; }
    const std::string& destination_zone() const { return destination_zone_; }
    ShipmentStatus status() const { return status_; }

    double weight_kg() const { return weight_kg_; }
    void set_weight_kg(double weight_kg) { weight_kg_ = weight_kg; }

    Cents quoted_rate_cents() const { return quoted_rate_cents_; }
    void set_quoted_rate_cents(Cents quoted_rate_cents) { quoted_rate_cents_ = quoted_rate_cents; }

    // The State component's only legal way to change status: advance()
    // checks the transition table, throws IllegalTransitionError for an
    // event that does not apply to the current status, and notifies every
    // subscribed observer only after a legal transition actually happens.
    void advance(ShipmentEvent event);

    // The seam Command's undo path uses. Restoring a prior status is not
    // a new business event, so it bypasses the transition table entirely,
    // but it still notifies observers: a dispatcher watching this shipment
    // needs to know a correction happened just as much as it needs to know
    // about a forward move.
    void restore_status_for_undo(ShipmentStatus status);

    void subscribe(LifecycleObserver& observer);
    void unsubscribe(LifecycleObserver& observer);

private:
    void notify(ShipmentStatus from, ShipmentStatus to);

    std::string id_;
    std::string carrier_name_;
    std::string destination_zone_;
    double weight_kg_;
    Cents quoted_rate_cents_;
    ShipmentStatus status_ = ShipmentStatus::Created;
    std::vector<LifecycleObserver*> observers_;
};

}  // namespace finalproject31

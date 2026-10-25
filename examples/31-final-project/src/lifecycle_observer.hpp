#pragma once

#include "shipment_types.hpp"

namespace finalproject31 {

class Shipment;

// Observer: a subject (Shipment) fans a status change out to every
// independently subscribed listener. A listener never learns about the
// others, and Shipment never learns what any listener does with the
// notification, only that it fired one.
class LifecycleObserver {
public:
    virtual ~LifecycleObserver() = default;

    virtual void on_transitioned(const Shipment& shipment, ShipmentStatus from, ShipmentStatus to) = 0;
};

}  // namespace finalproject31

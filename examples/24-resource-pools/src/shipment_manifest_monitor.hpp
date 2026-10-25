#pragma once

#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace pool24 {

// A monitor object: the manifest for one shipment owns its own mutex, and
// every public method locks it before touching state and unlocks on
// return, including when an exception unwinds past the lock_guard. A
// caller never sees, names, or forgets to take that mutex; the object
// serializes access to itself. Multiple warehouse-scan threads can call
// add_item() concurrently on the same manifest and never race.
//
// The distinction from the active object below: a monitor object still
// runs each call on the *caller's* thread, just one caller at a time. An
// active object instead moves the call onto a thread of its own. Both
// exist in this chapter because real systems need both shapes, serialize
// access to shared state (monitor) versus decouple invocation from
// execution (active object), and conflating them is a common design
// mistake.
class ShipmentManifestMonitor {
public:
    explicit ShipmentManifestMonitor(std::string shipment_id) : shipment_id_(std::move(shipment_id)) {}

    void add_item(std::string sku, double weight_kg) {
        std::lock_guard lock(mutex_);
        items_.emplace_back(std::move(sku), weight_kg);
    }

    // Returns true if an item with that sku was present and removed.
    bool remove_item(const std::string& sku) {
        std::lock_guard lock(mutex_);
        for (auto it = items_.begin(); it != items_.end(); ++it) {
            if (it->first == sku) {
                items_.erase(it);
                return true;
            }
        }
        return false;
    }

    double total_weight_kg() const {
        std::lock_guard lock(mutex_);
        double total = 0.0;
        for (const auto& [sku, weight] : items_) {
            total += weight;
        }
        return total;
    }

    std::size_t item_count() const {
        std::lock_guard lock(mutex_);
        return items_.size();
    }

    // A consistent point-in-time copy of every line item. Returned by
    // value, under the lock, so the caller's view cannot be torn by a
    // concurrent add_item()/remove_item() mid-copy.
    std::vector<std::pair<std::string, double>> snapshot() const {
        std::lock_guard lock(mutex_);
        return items_;
    }

    const std::string& shipment_id() const { return shipment_id_; }

private:
    mutable std::mutex mutex_;
    std::string shipment_id_;
    std::vector<std::pair<std::string, double>> items_;
};

}  // namespace pool24

#include "shipment_alert_notifier.hpp"

#include <utility>

namespace breakdep28 {

ShipmentAlertNotifier::ShipmentAlertNotifier(std::string supervisor_email, std::shared_ptr<AlertChannel> channel)
    : supervisor_email_(std::move(supervisor_email)), channel_(std::move(channel)) {}

void ShipmentAlertNotifier::flag_for_manual_review(const std::string& shipment_id, const std::string& reason) const {
    channel_->send(supervisor_email_, "Shipment " + shipment_id + " flagged for manual review: " + reason);
}

}  // namespace breakdep28

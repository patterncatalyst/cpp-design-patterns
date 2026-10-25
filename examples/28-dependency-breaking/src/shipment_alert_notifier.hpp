#pragma once

// Parameterize Constructor, built on top of the Extract Interface pass in
// alert_channel.hpp. The inherited ShipmentAlertNotifier had a single
// no-argument-dependency constructor that built its own SmtpAlertSender
// internally, with no way for a caller, including a test, to supply
// anything else. Adding a defaulted second parameter opens a seam without
// breaking a single existing call site: `ShipmentAlertNotifier
// notifier("ops@example.com");` still compiles, still talks to a real
// SmtpAlertSender, and still behaves exactly the way the inherited
// no-arg-dependency version did.

#include <memory>
#include <string>

#include "alert_channel.hpp"

namespace breakdep28 {

class ShipmentAlertNotifier {
public:
    explicit ShipmentAlertNotifier(
        std::string supervisor_email,
        std::shared_ptr<AlertChannel> channel = std::make_shared<SmtpAlertSender>("mail.internal.example"));

    void flag_for_manual_review(const std::string& shipment_id, const std::string& reason) const;

private:
    std::string supervisor_email_;
    std::shared_ptr<AlertChannel> channel_;
};

}  // namespace breakdep28

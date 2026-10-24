#include "shipment_audit_log_singleton.hpp"

#include <utility>

namespace shipping::audit {

ShipmentAuditLog& ShipmentAuditLog::instance() {
    static ShipmentAuditLog log;
    return log;
}

void ShipmentAuditLog::record(std::string event) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(std::move(event));
}

std::vector<std::string> ShipmentAuditLog::entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

void ShipmentAuditLog::reset_for_testing() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

}  // namespace shipping::audit

#include "delivery_auditor.hpp"

#include "notification_domain.hpp"

namespace notify {

bool DeliveryAuditor::confirmed_delivered(const std::string& dispatch_id) const {
    return status_provider_.status_of(dispatch_id) == DispatchStatus::Sent;
}

}  // namespace notify

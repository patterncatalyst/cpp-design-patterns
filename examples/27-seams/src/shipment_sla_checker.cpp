#include "shipment_sla_checker.hpp"

namespace seam27 {

bool ShipmentSlaChecker::is_breached(const ShipmentSla& sla) const {
    return clock_.get().now() >= sla.deadline;
}

#if __cplusplus >= 202302L
std::expected<std::chrono::minutes, std::chrono::minutes> ShipmentSlaChecker::try_time_remaining(
    const ShipmentSla& sla) const {
    const auto now = clock_.get().now();
    if (now >= sla.deadline) {
        return std::unexpected<std::chrono::minutes>(std::chrono::duration_cast<std::chrono::minutes>(now - sla.deadline));
    }
    return std::chrono::duration_cast<std::chrono::minutes>(sla.deadline - now);
}
#else
std::optional<std::chrono::minutes> ShipmentSlaChecker::try_time_remaining(const ShipmentSla& sla) const {
    const auto now = clock_.get().now();
    if (now >= sla.deadline) {
        return std::nullopt;
    }
    return std::chrono::duration_cast<std::chrono::minutes>(sla.deadline - now);
}
#endif

}  // namespace seam27

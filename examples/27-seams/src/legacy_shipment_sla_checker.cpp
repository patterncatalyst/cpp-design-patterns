#include "legacy_shipment_sla_checker.hpp"

#include <chrono>

namespace seam27 {

bool LegacyShipmentSlaChecker::is_breached(const ShipmentSla& sla) const {
    return std::chrono::system_clock::now() > sla.deadline;
}

}  // namespace seam27

#pragma once

#include "shipment_sla.hpp"

namespace seam27 {

// Inherited exactly as found in the shipping batch job: no constructor
// argument, no setter, and no seam of any kind. is_breached() reaches
// straight into std::chrono::system_clock::now() from inside the method
// body, so nothing outside this class, and no test, can make it believe
// the moment is anything other than the real wall clock reading at the
// instant it runs. This class stays in the codebase unmodified; it is the
// reference the characterization tests pin, and the AFTER checker in
// shipment_sla_checker.hpp is judged against it.
class LegacyShipmentSlaChecker {
public:
    bool is_breached(const ShipmentSla& sla) const;
};

}  // namespace seam27

#include "shipment_eta_scheduler.hpp"

namespace breakdep28 {

std::chrono::system_clock::time_point ShipmentEtaScheduler::current_time() const {
    return std::chrono::system_clock::now();
}

#if __cplusplus >= 202302L
std::chrono::system_clock::time_point DeducingEtaScheduler::current_time() const {
    return std::chrono::system_clock::now();
}
#endif

}  // namespace breakdep28

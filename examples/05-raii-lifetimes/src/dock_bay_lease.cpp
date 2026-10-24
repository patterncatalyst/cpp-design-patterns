#include "dock_bay_lease.hpp"

#include <utility>

namespace warehouse {

BayUnavailableError::BayUnavailableError(std::size_t bay_id)
    : std::runtime_error("dock bay " + std::to_string(bay_id) + " is already leased") {}

DockBayLease::DockBayLease(DockBayRegistry& registry, std::size_t bay_id)
    : registry_(&registry), bay_id_(bay_id), owns_bay_(false) {
    if (!registry_->try_lease(bay_id_)) {
        throw BayUnavailableError(bay_id_);
    }
    owns_bay_ = true;
}

DockBayLease::DockBayLease(DockBayRegistry& registry, std::size_t bay_id, AlreadyLeasedTag)
    : registry_(&registry), bay_id_(bay_id), owns_bay_(true) {}

DockBayLease::DockBayLease(DockBayLease&& other) noexcept
    : registry_(other.registry_), bay_id_(other.bay_id_), owns_bay_(other.owns_bay_) {
    // Leave the source with nothing to release, so its destructor runs
    // as a safe no-op instead of releasing a bay this object now owns.
    other.owns_bay_ = false;
}

DockBayLease& DockBayLease::operator=(DockBayLease&& other) noexcept {
    if (this != &other) {
        // This object may already own a different bay; release it before
        // taking over the source's bay, the same way a smart pointer's
        // move assignment frees its current resource before adopting a
        // new one.
        if (owns_bay_) {
            registry_->release(bay_id_);
        }
        registry_ = other.registry_;
        bay_id_ = other.bay_id_;
        owns_bay_ = other.owns_bay_;
        other.owns_bay_ = false;
    }
    return *this;
}

DockBayLease::~DockBayLease() {
    if (owns_bay_) {
        registry_->release(bay_id_);
    }
}

std::size_t DockBayLease::bay_id() const {
    return bay_id_;
}

bool DockBayLease::owns_bay() const {
    return owns_bay_;
}

#if __cplusplus >= 202302L
std::expected<DockBayLease, std::string> DockBayLease::try_acquire(DockBayRegistry& registry, std::size_t bay_id) {
    if (!registry.try_lease(bay_id)) {
        return std::unexpected<std::string>("dock bay " + std::to_string(bay_id) + " is already leased");
    }
    return DockBayLease(registry, bay_id, AlreadyLeasedTag{});
}
#endif

}  // namespace warehouse

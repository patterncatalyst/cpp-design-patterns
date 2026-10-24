#include "dock_bay_registry.hpp"

namespace warehouse {

DockBayRegistry::DockBayRegistry(std::size_t bay_count) : leased_(bay_count, false), release_count_(0) {}

bool DockBayRegistry::try_lease(std::size_t bay_id) {
    if (leased_.at(bay_id)) {
        return false;
    }
    leased_[bay_id] = true;
    return true;
}

void DockBayRegistry::release(std::size_t bay_id) {
    if (leased_.at(bay_id)) {
        leased_[bay_id] = false;
        ++release_count_;
    }
}

bool DockBayRegistry::is_leased(std::size_t bay_id) const {
    return leased_.at(bay_id);
}

std::size_t DockBayRegistry::bay_count() const {
    return leased_.size();
}

std::size_t DockBayRegistry::release_count() const {
    return release_count_;
}

}  // namespace warehouse

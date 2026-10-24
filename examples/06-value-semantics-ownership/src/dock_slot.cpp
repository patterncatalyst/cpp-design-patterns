#include "dock_slot.hpp"

#include <utility>

namespace warehouse {

void DockSlot::assign(std::unique_ptr<Parcel> parcel) {
    parcel_ = std::move(parcel);
}

std::unique_ptr<Parcel> DockSlot::release() {
    return std::move(parcel_);
}

bool DockSlot::occupied() const {
    return parcel_ != nullptr;
}

const Parcel* DockSlot::peek() const {
    return parcel_.get();
}

void transfer_parcel(DockSlot& from, DockSlot& to) {
    to.assign(from.release());
}

}  // namespace warehouse

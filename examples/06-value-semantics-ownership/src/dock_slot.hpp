#pragma once

#include <memory>

#include "parcel.hpp"

namespace warehouse {

// A single dock position in the warehouse. A DockSlot holds at most one
// Parcel at a time, and it holds that Parcel outright: nothing else can
// share it, and nothing else can outlive it. std::unique_ptr enforces
// that ownership at the type level, so no destructor call or manual
// delete has to appear anywhere a slot is cleared or reassigned.
class DockSlot {
public:
    // Takes ownership of parcel. Any parcel this slot already held is
    // destroyed first, the same way reassigning a unique_ptr always
    // releases its previous target before adopting the new one.
    void assign(std::unique_ptr<Parcel> parcel);

    // Gives up ownership of this slot's parcel and returns it to the
    // caller. The slot is empty after this call.
    std::unique_ptr<Parcel> release();

    bool occupied() const;
    const Parcel* peek() const;

private:
    std::unique_ptr<Parcel> parcel_;
};

// Moves a parcel from one dock slot to another. After this call, `from`
// is empty and `to` owns the parcel; no copy of the parcel is made, and
// no parcel is ever owned by both slots at once.
void transfer_parcel(DockSlot& from, DockSlot& to);

}  // namespace warehouse

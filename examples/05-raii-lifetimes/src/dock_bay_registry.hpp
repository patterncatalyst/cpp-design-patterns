#pragma once

#include <cstddef>
#include <vector>

namespace warehouse {

// Models a fixed number of loading-dock bays at a distribution center.
// Each bay is either free or leased to exactly one labeling station at a
// time. DockBayLease is the only code in this example allowed to flip a
// bay back to free; the registry itself has no notion of RAII, so the
// contrast between "a bool flag a caller must remember to flip back" and
// "an object whose destructor flips it back" stays visible at the call
// site.
class DockBayRegistry {
public:
    explicit DockBayRegistry(std::size_t bay_count);

    // Marks the bay leased if it was free and returns true; returns false
    // without side effects if it was already leased. Never throws, so
    // DockBayLease's constructor can decide what "already leased" means
    // for its own callers (throw, or report a typed failure).
    bool try_lease(std::size_t bay_id);

    // Marks the bay free again. Idempotent: calling release() on a bay
    // that is already free does nothing and does not advance
    // release_count(), which is what lets a test prove a moved-from
    // DockBayLease's destructor performs no second release.
    void release(std::size_t bay_id);

    bool is_leased(std::size_t bay_id) const;
    std::size_t bay_count() const;

    // Counts only genuine leased-to-free transitions. Two destructors can
    // run on the same bay_id (a moved-from lease and the lease that moved
    // from it) without this counter moving twice, because the second
    // release() call is a no-op on an already-free bay.
    std::size_t release_count() const;

private:
    std::vector<bool> leased_;
    std::size_t release_count_;
};

}  // namespace warehouse

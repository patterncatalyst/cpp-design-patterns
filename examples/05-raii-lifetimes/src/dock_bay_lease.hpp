#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

#include "dock_bay_registry.hpp"

#if __cplusplus >= 202302L
#include <expected>
#endif

namespace warehouse {

// Thrown by DockBayLease's constructor when the requested bay is already
// leased to another labeling station.
class BayUnavailableError : public std::runtime_error {
public:
    explicit BayUnavailableError(std::size_t bay_id);
};

// RAII guard over exactly one dock bay. The constructor acquires the bay
// (or fails outright); the destructor releases it no matter how the
// owning scope exits, a normal return, an early return, or an exception
// unwinding the stack. Copying is deleted: two DockBayLease objects
// holding the same bay_id would both try to release it, which is exactly
// the double-release bug RAII exists to make unrepresentable. Moving is
// allowed and transfers ownership: the moved-from object is left with
// nothing to release, so its destructor is a safe no-op.
class DockBayLease {
public:
    DockBayLease(DockBayRegistry& registry, std::size_t bay_id);

    DockBayLease(const DockBayLease&) = delete;
    DockBayLease& operator=(const DockBayLease&) = delete;

    DockBayLease(DockBayLease&& other) noexcept;
    DockBayLease& operator=(DockBayLease&& other) noexcept;

    ~DockBayLease();

    std::size_t bay_id() const;

    // False only after this object has been moved from; every other
    // DockBayLease in this example owns its bay for its whole lifetime.
    bool owns_bay() const;

#if __cplusplus >= 202302L
    // C++23 refinement: acquire a bay without throwing, reporting a typed
    // reason for rejection instead. Guarded so the mandatory C++20 build
    // path never needs std::expected; under C++20 the only way to acquire
    // a bay is the throwing constructor above.
    static std::expected<DockBayLease, std::string> try_acquire(DockBayRegistry& registry, std::size_t bay_id);
#endif

private:
    // Tag type for the private constructor used once a bay is already
    // known to be leased, so try_acquire never calls try_lease() twice.
    struct AlreadyLeasedTag {};
    DockBayLease(DockBayRegistry& registry, std::size_t bay_id, AlreadyLeasedTag);

    DockBayRegistry* registry_;
    std::size_t bay_id_;
    bool owns_bay_;
};

}  // namespace warehouse

#pragma once

#include <utility>

namespace warehouse {

// A general-purpose scope-exit guard: runs a callable when the guard is
// destroyed, unless dismiss() was called first. This is the same RAII
// shape as DockBayLease, generalized from "release one specific
// resource" to "run one specific cleanup action," which is why it reads
// naturally as a rollback for anything that does not already have its own
// RAII type: a partially updated counter, a half-written log entry, or a
// batch of work that should undo itself on the way out unless it commits.
//
// Move-only, same reasoning as DockBayLease: copying would let two guards
// run the same action twice.
template <typename Action>
class ScopeGuard {
public:
    explicit ScopeGuard(Action action) : action_(std::move(action)), active_(true) {}

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    ScopeGuard(ScopeGuard&& other) noexcept : action_(std::move(other.action_)), active_(other.active_) {
        other.active_ = false;
    }
    ScopeGuard& operator=(ScopeGuard&&) = delete;

    ~ScopeGuard() {
        if (active_) {
            action_();
        }
    }

    // Cancels the pending action. Call this once the operation the guard
    // was protecting has committed successfully and no longer needs its
    // rollback to run.
    void dismiss() {
        active_ = false;
    }

private:
    Action action_;
    bool active_;
};

// Deduces Action from the argument so callers write
// `auto guard = on_scope_exit([] { ... });` without naming the lambda's
// type, which C++20 cannot name anyway.
template <typename Action>
ScopeGuard<Action> on_scope_exit(Action action) {
    return ScopeGuard<Action>(std::move(action));
}

}  // namespace warehouse

---
title: "RAII and deterministic lifetimes"
order: 5
part: "cpp-substrate"
description: "Tying a resource's lifetime to an object's lifetime instead of a bool flag a caller has to remember: acquire in the constructor, release in the destructor, exception-safe by construction, taught against a warehouse dock-bay lease and a general-purpose scope-exit utility."
duration: "25 min"
---

## Learning objectives

By the end of this chapter you can:

- State RAII as a lifetime binding: a resource is held exactly as long as the object that owns it exists.
- Write a guard that acquires in its constructor and releases in its destructor, and say why that holds when the scope exits by exception.
- Make a guard move-only, and say what copying would break.
- Write a scope-exit utility for cleanup that has no guard type of its own, with a commit path that cancels the cleanup.
- Choose between a throwing constructor and a non-throwing factory that returns a typed failure.
- Recognize a resource whose lifetime has to outlive the scope that created it.

## Why this matters

A distribution center has a fixed number of loading-dock bays, and one labeling station uses a bay at a time. The naive version keeps a `bay_in_use[4]` array that a labeling function sets to `true` on entry and back to `false` before it returns. That holds for the happy path. An early return for a malformed shipment, or a validation error that throws three items into a batch, each needs its own `bay_in_use[id] = false`, and a reviewer has to trace every branch to confirm none forgot it. Miss one and the bay stays busy for good.

RAII, Resource Acquisition Is Initialization, ties the bay's "in use" state to an object. The constructor marks the bay busy and the destructor marks it free. Every local object's destructor runs when its scope ends, by normal return, early return or exception, so the language does the bookkeeping at every exit.

C++ destructors run at a specified moment: the instant the scope ends, not when a collector gets to them. A `finally` block or a `using` statement in another language gets the same guarantee at each call site; RAII puts it in the type, so every caller gets it without writing anything.

{% include excalidraw.html name="05-raii-lifecycle" caption="Figure 5.1: RAII lifecycle: acquire in the constructor, release in the destructor. A thrown exception during use still funnels into the same release, the destructor running during stack unwinding exactly as it would on a normal return." %}

## The domain: a dock bay lease

The code is in `examples/05-raii-lifetimes`. `DockBayRegistry` is the plain state a guard wraps, the way a file descriptor sits under `std::fstream`:

```cpp
class DockBayRegistry {
public:
    explicit DockBayRegistry(std::size_t bay_count);
    bool try_lease(std::size_t bay_id);
    void release(std::size_t bay_id);
    bool is_leased(std::size_t bay_id) const;
    std::size_t release_count() const;

private:
    std::vector<bool> leased_;
    std::size_t release_count_;
};
```

| Method | Behavior |
| --- | --- |
| `try_lease(bay_id)` | marks a free bay leased and returns `true`; returns `false` with no side effect if it's already leased |
| `release(bay_id)` | marks the bay free; idempotent |
| `release_count()` | counts leased-to-free transitions only |

`is_leased() == false` after a guard destructs can't tell a single release from a double one. `release_count()` can, and that matters once moves enter the picture.

## The guard: acquire in the constructor, release in the destructor

`DockBayLease`'s constructor calls `try_lease()` and throws `BayUnavailableError` if the bay is taken. Its destructor releases the bay it owns:

```cpp
class DockBayLease {
public:
    DockBayLease(DockBayRegistry& registry, std::size_t bay_id);
    DockBayLease(const DockBayLease&) = delete;
    DockBayLease& operator=(const DockBayLease&) = delete;
    DockBayLease(DockBayLease&& other) noexcept;
    DockBayLease& operator=(DockBayLease&& other) noexcept;
    ~DockBayLease();

    std::size_t bay_id() const;
    bool owns_bay() const;

private:
    DockBayRegistry* registry_;
    std::size_t bay_id_;
    bool owns_bay_;
};
```

There's no public `release()`. A bay held by a `DockBayLease` becomes free only when that object's lifetime ends, so "every leased bay is freed" becomes "every `DockBayLease` is destroyed," which the object model already guarantees.

## Exception safety: the destructor runs during unwinding

`label_shipment_batch` leases a bay and labels a batch. It can throw partway through:

```cpp
std::size_t label_shipment_batch(DockBayRegistry& registry, std::size_t bay_id, const std::vector<ShipmentItem>& items,
                                  std::size_t& labels_printed) {
    // Acquired here, released automatically on every exit path below:
    // the final return, or the throw inside the loop unwinding the stack
    // straight through this function.
    DockBayLease lease(registry, bay_id);

    labels_printed = 0;

    // Rolls labels_printed back to zero unless the batch reaches
    // dismiss() below. An exception thrown mid-loop skips dismiss(), so
    // the rollback runs during unwinding and the caller's out-parameter
    // never reports a partial count.
    auto rollback_count = on_scope_exit([&labels_printed] { labels_printed = 0; });

    for (const auto& item : items) {
        if (!item.barcode_is_valid) {
            throw InvalidShipmentError(item.tracking_id);
        }
        ++labels_printed;
    }

    rollback_count.dismiss();
    return labels_printed;
}
```

If the second of five items has a bad barcode, the function never reaches `return`. A thrown exception doesn't skip destructors: stack unwinding runs the destructor of every fully constructed local between the `throw` and the matching `catch`. `lease` releases the bay exactly as on a normal return, and the compiler inserted that call at every point control can leave the scope.

The function adds a second guarantee. A caller catching `InvalidShipmentError` shouldn't see `labels_printed` frozen at a partial count, because a rejected batch isn't a result. The `rollback_count` guard, covered below, resets it to zero unless the loop finishes.

## Move-only ownership

`DockBayLease` deletes its copy operations because a copy would be wrong. A copy and its source would both believe they own the same `bay_id`, and both destructors would release it. The idempotent registry hides that here; a file handle closed twice, a lock released after someone else acquired it, or a pointer deleted twice doesn't forgive it.

A move transfers ownership: the new object takes the source's state, and the source stops owning the bay.

```cpp
DockBayLease::DockBayLease(DockBayLease&& other) noexcept
    : registry_(other.registry_), bay_id_(other.bay_id_), owns_bay_(other.owns_bay_) {
    other.owns_bay_ = false;
}
```

The moved-from object's destructor still runs, but `owns_bay_` is `false`, so `if (owns_bay_) { registry_->release(bay_id_); }` does nothing. Move assignment first releases any bay the target already owns, then adopts the source's, the same order `std::unique_ptr`'s move assignment uses. After a move, both destructors run and `release_count()` advances by exactly one: the guarantee `std::unique_ptr` gives a pointer, that `delete` runs once however many owners it passed through.

{% include excalidraw.html name="05-move-ownership-transfer" caption="Figure 5.2: a move constructor hands ownership from one dock bay lease to another, and the registry's release count still advances by exactly one." %}

## A general-purpose scope guard

Most cleanup in a function has no dedicated type: resetting a counter, undoing a partial update, logging that an operation didn't finish. `ScopeGuard` applies the same shape, "run this when the scope ends," to any callable:

```cpp
template <typename Action>
class ScopeGuard {
public:
    explicit ScopeGuard(Action action) : action_(std::move(action)), active_(true) {}
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard&& other) noexcept;
    ~ScopeGuard() {
        if (active_) {
            action_();
        }
    }
    void dismiss() { active_ = false; }

private:
    Action action_;
    bool active_;
};

template <typename Action>
ScopeGuard<Action> on_scope_exit(Action action) {
    return ScopeGuard<Action>(std::move(action));
}
```

`dismiss()` cancels the cleanup once the operation has succeeded. In `label_shipment_batch` it runs only after every item is labeled; if the loop throws, the guard fires during unwinding and resets the count before the exception reaches the caller. That's commit-or-rollback in miniature: arm the undo, do the risky work, cancel the undo only once the work succeeded.

Several guards in one scope run in LIFO order, last constructed first, like every local variable. That's the object model, not `ScopeGuard`: it's why a function can take a lock and then a buffer and trust the buffer to go first.

## Typed acquisition failure: C++20 versus C++23

A constructor can only report failure by throwing, so every caller that wants to handle a busy bay without an exception handler still needs a `try` block. `std::expected`, new in C++23, gives a second acquisition path that returns the conflict as a value.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: no equivalent entry point exists in the mandatory build path.
// Acquiring a bay without throwing isn't available; a caller who wants to
// avoid exception handling has no typed-failure alternative to reach for.
DockBayLease lease(registry, bay_id);  // throws BayUnavailableError
```
```cpp
// C++23: std::expected carries a typed reason alongside the success
// value, checkable with an if and no exception machinery. This is an
// additional entry point, guarded behind #if __cplusplus >= 202302L, not
// a replacement for the throwing constructor above.
std::expected<DockBayLease, std::string> result = DockBayLease::try_acquire(registry, bay_id);
if (!result.has_value()) {
    std::cout << "Rejected: " << result.error() << '\n';
}
```

Both keep one lease per bay and release through the same destructor. Only the report changes: an exception the caller may catch, or a value the caller must check.

> **🤖 Explore with your agent**
>
> "In `examples/05-raii-lifetimes`, add a `LabelPrinterLease` beside `DockBayLease` that acquires a printer from a registry in its constructor and releases it in its destructor. Delete the copy constructor and copy assignment explicitly. Write a `noexcept` move constructor that clears the source's ownership flag, and a move assignment that releases the target's printer before adopting the source's. Use `on_scope_exit` from `src/scope_guard.hpp` for any rollback, and add a `try_acquire` returning `std::expected` behind `#if __cplusplus >= 202302L`."
>
> Check that `= delete` appears on both copy operations, and that a move followed by two destructors advances the registry's release count by one.

## When not to use a single-owner guard

RAII's one-object-one-resource shape fits when one owner's scope should bound the resource's lifetime. A bay leased for one labeling function fits. Other cases don't:

| Resource | Needs |
| --- | --- |
| held only for one function's work | a single-owner guard like `DockBayLease` |
| built in one function and kept by a longer-lived object | the same guard, moved into the owner |
| a pooled database connection handed to a background worker that outlives the dispatching function | shared ownership; a scoped guard would release it under the worker |
| queued for another thread, or stored where several holders must agree it's alive | shared ownership, the subject of Chapter 6 |

Forcing the single-owner shape onto a shared resource means picking one holder as the real owner and hoping every other reference outlives it only by coincidence.

## Build, run, observe

From the repository root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 15 cases pass under `cpp23` and 13 under `cpp20`; the two `std::expected` cases compile only under C++23. Then run the demo:

```sh
examples/05-raii-lifetimes/demo.sh cpp23
```

```
== Acquire in the constructor, release in the destructor ==
  Lease acquired for bay 0
  Bay 0 is leased
  Lease went out of scope.
  Bay 0 is free

== A second lease on an already-leased bay is rejected ==
  Rejected: dock bay 1 is already leased

== Move transfers ownership; the moved-from lease releases nothing ==
  original.owns_bay() = false
  moved.owns_bay()    = true
  releases recorded by this block (expect 1, not 2): 1

== A whole batch labels successfully; the bay releases at the end ==
  Labeled 3 shipments.
  Bay 3 is free

== A bad barcode partway through rolls back the count and releases the bay ==
  Rejected: shipment PKG-2002 has an invalid barcode
  labels_printed after rollback: 0
  Bay 3 is free

== ScopeGuard runs its actions in LIFO order, like stack destructors ==
  Run order (expect 3, 2, 1): 3 2 1 

== C++23 refinement: acquiring a bay without throwing ==
  Rejected: dock bay 1 is already leased
  Acquired bay 2 without an exception.
```

Under `cpp20` the demo prints the same output without the final `std::expected` block.

- **Forget to clear the source.** In a scratch copy, delete `other.owns_bay_ = false;` from the move constructor. Two cases fail: a move now releases twice, and a lease returned from `try_acquire` frees its bay as soon as the temporary it was moved from is destroyed, while the caller still holds it.

## For deeper coverage

- Bjarne Stroustrup, [*The C++ Programming Language*](https://www.informit.com/store/c-plus-plus-programming-language-9780321563842), 4th edition (Addison-Wesley): the resource-management chapters covering RAII, exception safety, and move semantics as a single, connected set of guarantees rather than three separate features.
- Scott Meyers, [*Effective Modern C++*](https://www.oreilly.com/library/view/effective-modern-c/9781491908419/) (O'Reilly): Items on move semantics, `noexcept`, and resource-managing classes, including the deleted-copy-operations pattern this chapter's `DockBayLease` and `ScopeGuard` both follow.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/05-raii-lifetimes`, which checks observable integer state, a release counter and a rolled-back label count, rather than just an exit code.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.

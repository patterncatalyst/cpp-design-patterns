---
title: "RAII and deterministic lifetimes"
order: 5
part: "cpp-substrate"
description: "Tying a resource's lifetime to an object's lifetime instead of a bool flag a caller has to remember: acquire in the constructor, release in the destructor, exception-safe by construction, taught against a warehouse dock-bay lease and a general-purpose scope-exit utility."
duration: "25 min"
---

## Learning objectives

By the end of this chapter you can:

- State RAII as a lifetime binding, not a cleanup convention: a resource is held for exactly as long as an object that owns it exists, never longer and never shorter.
- Write a resource guard that acquires in its constructor and releases in its destructor, and explain why that guarantee holds even when the owning scope exits through a thrown exception.
- Make a resource guard move-only, and explain what copying it would break that moving it does not.
- Write a general-purpose scope-exit utility for cleanup that has no dedicated guard type of its own, including a commit path that cancels the cleanup once an operation has succeeded.
- Choose between a throwing constructor and a non-throwing, typed-failure factory for acquiring a resource, and say which standard each belongs to.
- Recognize when a resource's lifetime needs to outlive the scope that created it, and say what to reach for instead of a single-owner guard.

## Why this matters

A distribution center runs a fixed number of loading-dock bays, and exactly one labeling station can use a bay at a time. A naive implementation tracks this with a bool: a `bay_in_use[4]` array a labeling function sets to `true` on entry and sets back to `false` before it returns. That works for the happy path. It stops working the moment the function has more than one exit: an early return for a malformed shipment, or a validation error three items into a batch that throws partway through. Every one of those paths needs its own matching `bay_in_use[id] = false`, and a reviewer has to trace every branch by hand to confirm none of them forgot it. Miss one, and the bay stays marked busy forever, with no process left running to free it.

RAII, Resource Acquisition Is Initialization, is C++'s answer to that exact failure mode, and Chapter 1 already gave this book's general name for the force behind it: resources need releasing no matter how a scope exits, and a programmer re-deriving the correct release at every exit point is a job the compiler can do instead. The fix is not a better bool flag or a `try`/`finally` block C++ does not have. It is tying the bay's "in use" state to the lifetime of an object. The object's constructor marks the bay busy; its destructor marks the bay free. The C++ language's own rule, every local object's destructor runs when its scope ends, whether that end is a normal return, an early return, or a thrown exception unwinding the stack, now does the bookkeeping that used to be a manual chore scattered across every exit path.

This matters more in C++ than in a garbage-collected language precisely because C++ gives destructors a deterministic, specified moment to run: the instant an object's scope ends, not whenever a collector gets around to it. A `finally` block or a `using` statement in another language reaches for the same guarantee at the call site; RAII builds it into the type itself, so every caller gets it automatically, without writing a `finally` block of their own.

{% include excalidraw.html name="05-raii-lifecycle" caption="Figure 5.1: RAII lifecycle: acquire in the constructor, release in the destructor. A thrown exception during use still funnels into the same release, the destructor running during stack unwinding exactly as it would on a normal return." %}

## The domain: a dock bay lease

The example behind this chapter lives in `examples/05-raii-lifetimes`. `DockBayRegistry` models a fixed number of dock bays, each either free or leased, with two non-throwing bookkeeping methods: `try_lease(bay_id)` marks a free bay leased and returns `true`, or returns `false` without side effects if it was already leased; `release(bay_id)` marks a bay free again, and is idempotent, calling it on an already-free bay does nothing. The registry itself has no notion of RAII. It is the plain state a guard object wraps, the same way a raw file descriptor or a raw pointer is the plain state `std::unique_ptr` or `std::fstream` wrap underneath their own RAII guarantees.

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

`release_count()` exists for exactly one reason: to give this chapter's test suite an observable number it can use to prove a negative. Checking `is_leased(bay_id) == false` after a guard destructs is necessary, but it cannot tell you whether the bay was released once or released twice and happened to land on `false` either way. A counter that advances only on a leased-to-free transition can, and that distinction is exactly what matters once move semantics enter the picture below.

## The guard: acquire in the constructor, release in the destructor

`DockBayLease` is the RAII guard itself. Its constructor takes a registry reference and a bay id, calls `try_lease()`, and throws `BayUnavailableError` if the bay was already taken. Its destructor calls `release()` unconditionally, with no condition to get wrong at the call site:

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

A caller never calls `release()` directly. There is no public method for it. The only way a bay a `DockBayLease` holds ever becomes free again is for that `DockBayLease` object to reach the end of its own lifetime, which means the correctness of "every leased bay eventually frees" has moved from "every code path remembered to call release" to "every `DockBayLease` object eventually destructs," and the second claim is one the C++ object model already guarantees. Using a `DockBayLease` to label a batch of shipments looks like this, with no explicit cleanup visible anywhere in the body:

```cpp
std::size_t label_shipment_batch(DockBayRegistry& registry, std::size_t bay_id,
                                  const std::vector<ShipmentItem>& items,
                                  std::size_t& labels_printed) {
    DockBayLease lease(registry, bay_id);
    labels_printed = 0;
    for (const auto& item : items) {
        if (!item.barcode_is_valid) {
            throw InvalidShipmentError(item.tracking_id);
        }
        ++labels_printed;
    }
    return labels_printed;
}
```

## Exception safety: the destructor runs during unwinding too

That loop can throw partway through a batch, on the second item of five, say, and `label_shipment_batch` never reaches its `return` statement. The `lease` local variable still goes out of scope on the way out, because a thrown exception does not skip destructors; it runs every destructor for every fully constructed local object between the `throw` and the nearest matching `catch`, a process the standard calls stack unwinding. `lease`'s destructor runs exactly as it would on a normal return, and the bay releases.

This is the property a bool flag cannot give you without a second line of code at every exit point, and the property a `finally` block in another language exists to recreate. In C++, `~DockBayLease()` already gets invoked at scope exit by the language itself, with no block to write and no branch to remember, because the compiler inserts the destructor call at every point control can leave the scope, including the ones a thrown exception creates. The test suite behind this chapter proves this directly: a lambda acquires a lease and then throws, the test catches the exception, and asserts the registry shows the bay free afterward. The assertion passes because the object model, not the test's own cleanup code, is what released it.

`label_shipment_batch` adds a second guarantee on top of the first, using a utility this chapter introduces below. `labels_printed` is an out-parameter the caller supplies, updated as each item labels successfully. If the loop throws, a caller catching `InvalidShipmentError` should not see a count frozen at whatever partial progress the batch made before failing; a half-labeled batch is not a result, it is a failure, and reporting "2 labels printed" for a batch that was rejected would mislead whatever downstream system reads that number. A `ScopeGuard`, covered next, rolls `labels_printed` back to zero unless the batch reaches its end.

## Move-only ownership: why copying a guard is the wrong default

`DockBayLease`'s copy constructor and copy assignment operator are both `= delete`. This is not a restriction added out of caution; it is the only correct choice available. If `DockBayLease` could be copied, a copy and its source would both believe they own the same `bay_id`, and both destructors would call `release()` on it. The second call happens to be harmless here, because `DockBayRegistry::release()` is idempotent, but the broader pattern this represents, two objects both believing they exclusively own one resource, is exactly the shape of a double-free bug against a resource that does not forgive a second release the way this registry does: a file handle closed twice, a lock released twice when a second holder has already acquired it, a `delete` run twice on the same pointer.

Moving a `DockBayLease` is different in kind, not just in degree. A move transfers ownership outright: the move constructor copies the source's state into the new object, then sets the source's `owns_bay_` flag to `false`, leaving the moved-from object with nothing to release.

```cpp
DockBayLease::DockBayLease(DockBayLease&& other) noexcept
    : registry_(other.registry_), bay_id_(other.bay_id_), owns_bay_(other.owns_bay_) {
    other.owns_bay_ = false;
}
```

After this runs, exactly one `DockBayLease` object believes it owns `bay_id_`, and that belief is correct. The moved-from object's destructor still runs when its own scope ends, calling the same `~DockBayLease()` body as any other instance, but `owns_bay_` is `false`, so the `if (owns_bay_) { registry_->release(bay_id_); }` guard inside it short-circuits into a no-op. Move assignment also releases any bay the target already owns before adopting the source's, a step the move constructor never needs because a freshly constructed object cannot already own something. That release-before-adopt order is the same sequencing `std::unique_ptr`'s move assignment uses when it frees its current pointee before taking ownership of a new one.

This chapter's test suite proves the "no double release" claim with a number, not just a boolean. `DockBayRegistry::release_count()` advances only on a leased-to-free transition, so a test that moves a `DockBayLease` and then lets both the moved and the moved-from objects destruct can assert `release_count() == 1`. Two destructors ran on the same `bay_id_`. Exactly one of them did real work. That is the quantitative version of the same guarantee `std::unique_ptr` gives a raw pointer: a move transfers responsibility for calling `delete` exactly once, never zero times and never twice, no matter how many `unique_ptr` objects the pointer passed through on its way to wherever it ends up.

## A general-purpose scope guard

`DockBayLease` is a resource guard custom-built for one specific resource. Most of the cleanup a function needs during its lifetime does not get a dedicated type: resetting a counter, undoing a partial update to a data structure, logging that an operation did not finish. `ScopeGuard` generalizes the same RAII shape, "run something when this scope ends," to any callable at all:

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

`dismiss()` is the piece a one-off resource guard like `DockBayLease` does not need but a rollback utility does: a way to say "the operation succeeded; cancel the cleanup I armed in case it didn't." `label_shipment_batch` uses exactly this shape. It arms a `ScopeGuard` right after resetting `labels_printed` to zero, whose action resets it to zero again, and calls `dismiss()` only after the loop finishes labeling every item:

```cpp
auto rollback_count = on_scope_exit([&labels_printed] { labels_printed = 0; });
for (const auto& item : items) {
    if (!item.barcode_is_valid) {
        throw InvalidShipmentError(item.tracking_id);
    }
    ++labels_printed;
}
rollback_count.dismiss();
```

If every item labels successfully, `dismiss()` runs before the guard's destructor ever gets a chance to fire, so `labels_printed` keeps its real count. If the loop throws, the function never reaches `dismiss()`, the guard's destructor runs during unwinding exactly like `DockBayLease`'s does, and the out-parameter resets to zero before the exception reaches the caller. This is the commit-or-rollback idiom RAII enables in miniature: arm the undo first, do the risky work second, cancel the undo only once the work provably succeeded.

`ScopeGuard` also demonstrates something `DockBayLease` alone cannot, because `DockBayLease` is typically the only guard active in a given scope: when more than one guard is alive in the same scope, their destructors run in LIFO order, last constructed, first destroyed, the same order C++ already uses for every local variable. A test in this chapter's suite declares three `ScopeGuard` objects in a row, each recording an id into a shared vector when it runs, and the vector comes back reversed from declaration order. That is not a property of `ScopeGuard`'s implementation; it is a property of the C++ object model that `ScopeGuard` inherits directly, the same ordering guarantee that lets a function acquire a lock and then a buffer and trust that the buffer releases before the lock does, without writing that sequencing anywhere explicitly.

## Typed acquisition failure: C++20 versus C++23

`DockBayLease`'s constructor reports a conflict the only way C++20 gives a constructor to report failure without an out-parameter: throwing. That is a reasonable default, and it is what `label_shipment_batch` relies on. It forces every caller who wants to handle a busy bay without an exception handler to wrap the call in a `try` block regardless of how likely they expect the conflict to be. C++23's `std::expected` gives this example a second acquisition path that reports the same conflict as a typed value instead of an exception.

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

Both columns protect the same invariant, one bay, one lease at a time, and both release through the same destructor once a `DockBayLease` exists. The only thing that changes between standards is how a caller learns that an acquisition did not happen: an exception a caller must choose to catch, or a value a caller must choose to check. Neither column touches the move-only ownership rule or the destructor's unconditional release; RAII's core guarantee does not depend on which standard reports an acquisition failure.

> **AI sidebar.** Ask an LLM pairing partner for "a wrapper class that manages a resource" and a common result reaches for the right shape, a constructor that acquires and a destructor that releases, but skips the move semantics this chapter spent two sections on, leaving the copy constructor and copy assignment operator implicitly defaulted instead of explicitly deleted. That compiles. It also means the generated type can be copied, and a copy plus its source will both release the same resource when they destruct, which is correct by accident until the resource is something less forgiving than an idempotent boolean flag, a file handle, a lock, a heap allocation. Read a generated resource guard for exactly this: does it delete its copy operations explicitly, or does it rely on them being implicitly deleted because a member happens to be move-only, which stops being true the moment someone adds an `int` member to track something else? A guard that compiles with a default copy constructor intact is the clearest sign to check before trusting it anywhere a resource matters.

## When not to use a single-owner RAII guard

RAII's one-object-one-resource shape is the right tool exactly when a resource has one clear owner whose scope should bound the resource's lifetime. It is the wrong tool the moment a resource's lifetime needs to outlive any single scope, or needs more than one simultaneous owner.

A dock bay leased for the duration of one labeling function is squarely in RAII's territory: the function that leases it is also the function that is done with it, and tying the two lifetimes together is strictly a win. A database connection pulled from a pool and handed to a background worker that outlives the function that dispatched it is not. If `DockBayLease`'s shape were forced onto that case, the connection would release the moment the dispatching function returned, out from under the worker still using it. What that case needs instead is shared ownership, a resource with more than one simultaneous stakeholder and a decision about when the last one releases it, which is exactly the subject Chapter 6 covers next: `std::shared_ptr` and the ownership models that apply once "exactly one scope owns this" stops being true.

The other sign a single-owner guard is the wrong fit is a resource that needs handing off across an asynchronous boundary, queued for a worker thread to pick up later, stored in a container that outlives the function that created the entry, or returned from a factory to a caller whose lifetime the factory cannot see. Move semantics handle the simple version of this, a lease constructed in one function and moved into a longer-lived object, exactly the way this chapter's `DockBayLease` already supports. They do not handle the harder version, where more than one piece of code needs to agree the resource is still alive before any of them releases it. That coordination problem is shared ownership's job, not a single move-only guard's, and reaching for `DockBayLease`'s shape there would mean picking one owner to be the "real" one and hoping every other reference only ever outlives it by coincidence.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/05-raii-lifetimes` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 15 cases under the `cpp23` preset, 13 under `cpp20`, the difference being the two `std::expected`-specific cases that only compile under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 15   # cpp23
100% tests passed, 0 tests failed out of 13   # cpp20
```

To see the lease, the exception path, and the `ScopeGuard` rollback run side by side rather than just reading an assertion that says they work, run the example's own demo script:

```sh
examples/05-raii-lifetimes/demo.sh cpp23
```

It leases and releases a bay normally, rejects a second lease on a bay that is already held, moves a lease and shows the registry's `release_count()` advance by exactly one rather than two, labels a batch successfully, labels a second batch with a bad barcode and shows both the label count rolling back to zero and the bay releasing during the exception, and finally acquires a bay through the C++23 `std::expected` path that reports a conflict as a value instead of an exception:

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

Running `examples/05-raii-lifetimes/demo.sh cpp20` prints the same output minus the final `std::expected` block, which only compiles under the `cpp23` preset.

## For deeper coverage

- Bjarne Stroustrup, *The C++ Programming Language*, 4th edition (Addison-Wesley): the resource-management chapters covering RAII, exception safety, and move semantics as a single, connected set of guarantees rather than three separate features.
- Scott Meyers, *Effective Modern C++* (O'Reilly): Items on move semantics, `noexcept`, and resource-managing classes, including the deleted-copy-operations pattern this chapter's `DockBayLease` and `ScopeGuard` both follow.
- *GoogleTest in Practice* (Hyuk Myeong, Manning) and the official GoogleTest documentation: the testing idiom behind `examples/05-raii-lifetimes`'s suite, which asserts observable integer state, a release counter and a rolled-back label count, rather than just checking that the binary exits zero.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.

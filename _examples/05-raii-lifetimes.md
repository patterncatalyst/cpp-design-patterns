---
layout: example
order: 5
title: "RAII and deterministic lifetimes"
description: A warehouse dock-bay lease that acquires in its constructor and always releases in its destructor, even on an exception.
github_path: examples/05-raii-lifetimes
card_eyebrow: "Chapter 5 · RAII"
card_title: "Dock-bay lease: RAII"
card_blurb: "A scoped resource guard that acquires in the constructor and releases in the destructor, stays correct through exceptions and moves, plus a general-purpose scope guard."
---

A warehouse loading-dock bay lease that shows RAII and deterministic object
lifetimes in a real setting:

- **`DockBayLease`** acquires a bay in its constructor and releases it in its
  destructor, so the bay is freed at scope exit no matter how the scope ends.
- **Exception safety.** When a bad barcode throws mid-batch, the destructor
  still runs during stack unwinding and the bay is released.
- **Move-only ownership.** Copy is deleted; move transfers ownership and the
  moved-from lease does not release twice (proven with a release counter).
- **`ScopeGuard`** is a general lambda-on-destruction utility with `dismiss()`
  to cancel the rollback on success.

A C++23 `try_acquire()` returning `std::expected` is guarded so the C++20 build
still works.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 15/15 tests pass
./examples/05-raii-lifetimes/demo.sh cpp23
```

The demo leases a bay, runs a batch, rolls back the label count on a thrown
bad barcode while still releasing the bay, and shows a moved lease releasing
exactly once. Chapter 5 walks through why the destructor is the release point.
Builds standalone and under the `cpp20` preset (13 tests).

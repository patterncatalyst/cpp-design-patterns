---
layout: example
order: 24
title: "Resource pools"
description: A generic bounded pool with an RAII lease handle, taught through a shipment-tracking connection pool, plus a thread pool, a monitor object, and an active object.
github_path: examples/24-resource-pools
card_eyebrow: "Chapter 24 · Resource pools"
card_title: "Connections: bounded and reused"
card_blurb: "A generic ResourcePool with an RAII lease that returns its resource on destruction, shown as a shipment-tracking connection pool, a label-rendering thread pool, a monitor object, and an active object that returns futures."
---

Five production resource-management shapes on a shipment-tracking domain:

- **Generic resource pool (lead).** `ResourcePool<Resource>` hands out a bounded
  set of expensive-to-build resources through an RAII `Lease` that returns the
  resource to the pool when it goes out of scope. Acquire blocks with a timeout
  when the pool is drained.
- **Connection pool.** The canonical instance: a bounded pool of tracking-store
  connections built behind a `ConnectionFactory` seam (mocked with GoogleMock).
- **Thread pool.** Worker threads drain a task queue to render shipping labels.
- **Monitor object.** A manifest whose own mutex serializes its public methods.
- **Active object.** A label printer with its own thread and request queue,
  returning `std::future` so the call is decoupled from execution.

The deep threading mechanics live in the companion *Concurrency and Parallelism*
book; this chapter teaches the pattern shapes and when to reach for each.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 19/19 tests pass
./examples/24-resource-pools/demo.sh cpp23
```

The demo leases connections past the pool bound to show acquire blocking and
releasing, fans label-render work across the thread pool, and submits print jobs
to the active object. A C++23 `try_acquire` returning `std::expected` names why
an acquire failed; the C++20 build falls back to `std::optional`. Builds
standalone and under the `cpp20` preset (18 tests).

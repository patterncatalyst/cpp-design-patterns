---
title: "Resource pools"
order: 24
part: "patterns-in-the-wild"
description: "A generic, bounded resource pool with an RAII lease handle, taught against a shipment-tracking connection pool, plus three sibling concurrency shapes from the same domain: a thread pool rendering shipping labels, a monitor object, and an active object."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Build a generic `ResourcePool<Resource>` that bounds how many expensive resources exist, with a nested `Lease` RAII handle that returns its resource on every exit path.
- Build a connection pool on that mechanism, with a blocking `acquire()` that honors a timeout.
- Build a thread pool whose workers drain a shared queue, and explain the allocation `std::move_only_function` removes.
- Build a monitor object that serializes every public method on its own mutex.
- Build an active object whose methods enqueue a request and return a `std::future`.
- Pick among these shapes for a concrete scenario.

## Why this matters

A shipment-tracking service doesn't open a fresh connection to its tracking store for every scan event. Opening one costs a DNS lookup, a TCP handshake, often TLS and an authentication round trip: milliseconds, against a query that takes microseconds once connected. A pool builds a bounded set of connections once, hands them out and takes them back. The pattern isn't in the 1994 Gang of Four catalogue; Schmidt, Stal, Rohnert and Buschmann's *Pattern-Oriented Software Architecture, Volume 2* formalized it alongside Monitor Object and Active Object.

All four shapes bound or serialize access to something expensive or unsafe to share. They don't re-teach `std::mutex`, `std::condition_variable`, `std::jthread` or the memory model; the sibling book [*C++ Concurrency and Parallelism*](https://patterncatalyst.github.io/cpp-concurrency-and-parallelism/) covers those, including lock-free structures, memory ordering and false sharing. Here the subject is the shape each pattern gives a caller.

{% include excalidraw.html name="24-resource-pools" caption="Figure 24.1: the generic ResourcePool and PoolLease, the TrackingStoreConnectionPool built on them, and three related concurrency shapes, a thread pool, a monitor object, and an active object, that share the same bounded-resource and serialized-access concerns." %}

The tracking service has four kinds of work:

| Work | Pressure | Pattern |
| --- | --- | --- |
| Recording a tracking event in the store | The connection is expensive to open, cheap to reuse | Connection pool |
| Rendering a shipping label | CPU-bound; many requests at once | Thread pool |
| Updating a package manifest during intake | Several scan threads touch the same state | Monitor object |
| Printing a label on a physical printer | One at a time, on its own timeline; callers shouldn't wait | Active object |

## Pattern: Resource pool

### The problem

Every request needs a tracking-store connection for microseconds, and building one takes milliseconds. Building one per request spends more on setup than on work, and an unbounded number of connections exceeds what the store accepts.

### The solution

A **resource pool** builds a bounded number of expensive resources, lends each to one caller at a time, and takes it back for reuse when the caller is done.

### How C++ models it

`ResourcePool<Resource>` in `src/resource_pool.hpp` holds a bounded free list of `std::unique_ptr<Resource>`, builds resources lazily up to its capacity and hands them out through a nested `Lease`:

```cpp
template <typename Resource>
class ResourcePool {
public:
    using Factory = std::function<std::unique_ptr<Resource>()>;

    ResourcePool(std::size_t capacity, Factory factory);

    class Lease {
    public:
        Resource& operator*() const { return *resource_; }
        Resource* operator->() const { return resource_.get(); }
        // move-only; destructor returns the resource to the pool
    };

    Lease acquire(std::chrono::milliseconds timeout);
    // ...
};
```

`acquire()` waits under a `std::unique_lock` with the predicate form of `condition_variable::wait_for` until a resource is free, the pool can build another under its capacity, or the timeout elapses. It throws `std::runtime_error` on timeout or after `shutdown()`.

### The lease is the only way back

`Lease` is move-only: a copy would let two callers each believe they own the same connection. Its destructor pushes the resource back onto the free list whether the scope ends normally, through an early `return` or by exception. There's no `release()` method to forget.

```cpp
{
    auto lease = pool.acquire(100ms);
    lease->record_event("SHM-1001", "in_transit");
}  // lease destructor runs here: the connection returns to the pool
```

The bound is what separates a pool from a cache. A cache grows to fit demand; a pool caps it at what the backing resource sustains: a connection limit, a thread count, a licensed hardware seat.

### The connection pool

`TrackingStoreConnectionPool` wraps `ResourcePool<TrackingStoreConnection>` so call sites depend on a named domain type, and forwards `acquire()`, `try_acquire()` and `shutdown()` unchanged. Its constructor takes a `std::shared_ptr<ConnectionFactory>`, the seam a GoogleMock factory replaces:

```cpp
class ConnectionFactory {
public:
    virtual ~ConnectionFactory() = default;
    virtual std::unique_ptr<TrackingStoreConnection> create() = 0;
};
```

With a mock factory expecting `create()` `.Times(2)`, ten acquire-and-release cycles on a capacity-2 pool build exactly two connections. A change that builds a connection per request fails that expectation.

### Why `try_acquire()` failed

`try_acquire()` never waits. Under C++20 an empty `std::optional` can't say whether every connection is leased or the pool was shut down. `std::expected`, new in C++23, carries a `PoolAcquireError` of `Exhausted` or `Closed`; the code sits behind `#if __cplusplus >= 202302L`, so the C++20 build never needs `<expected>`.

{% include codetabs.html langs="C++20|C++23" %}
```cpp
// C++20: a null optional is the only signal available. The caller learns
// *that* a non-blocking attempt failed, never *why* it failed.
std::optional<TrackingStoreConnectionPool::Lease> lease = pool.try_acquire();
if (!lease.has_value()) {
    // was every connection leased, or did someone call shutdown()? this
    // line cannot say.
}
```
```cpp
// C++23: std::expected carries a typed reason alongside the failure,
// checkable with an if and no exception machinery.
std::expected<TrackingStoreConnectionPool::Lease, PoolAcquireError> lease = pool.try_acquire();
if (!lease.has_value()) {
    std::cout << "Rejected: " << describe(lease.error()) << '\n';
}
```

## Pattern: Thread pool

### The problem

Several requests want labels rendered at once. A thread per request costs a thread start each time and oversubscribes the cores; one thread makes every request wait behind the others.

### The solution

A **thread pool** starts a fixed set of worker threads once and has them pull tasks from a shared queue until told to stop.

### How C++ models it

`LabelRenderThreadPool::submit()` wraps the work in a `std::packaged_task`, queues it and returns the `std::future` at once:

```cpp
std::future<RenderedLabel> submit(ShippingLabelRequest request);
```

The queue accepts unrelated work, so it stores a type-erased callable. `std::packaged_task` is move-only, and that's where the standards differ:

{% include codetabs.html langs="C++20|C++23" %}
```cpp
// C++20: std::function requires its target to be copy-constructible.
// std::packaged_task is move-only, so it cannot sit inside a std::function
// directly. Wrapping it in a shared_ptr, which IS copyable, is the usual
// workaround, at the cost of one extra heap allocation and reference
// count per submitted task.
using Task = std::function<void()>;

auto shared_task = std::make_shared<std::packaged_task<RenderedLabel()>>(std::move(packaged));
Task task = [shared_task] { (*shared_task)(); };
```
```cpp
// C++23: std::move_only_function stores the move-only packaged_task
// directly. No extra heap allocation beyond the packaged_task's own, and
// no reference count to maintain.
using Task = std::move_only_function<void()>;

Task task(std::move(packaged));
```

A C++20 pool either pays the `shared_ptr` allocation per task or hand-rolls a move-only eraser. `std::move_only_function`, new in C++23, removes both. The two builds behave the same: every submitted task runs exactly once.

### Stopping without a flag

`std::jthread` and `std::stop_token` replace a hand-kept "please stop" flag:

```cpp
void LabelRenderThreadPool::worker_loop(std::stop_token stop_token) {
    while (true) {
        Task task;
        {
            std::unique_lock lock(mutex_);
            if (!cv_.wait(lock, stop_token, [this] { return !tasks_.empty(); })) {
                return;  // stop requested, queue empty: nothing left to drain
            }
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        task();
    }
}
```

`cv_` is a `std::condition_variable_any`, whose stop-token overload of `wait()` wakes every waiting worker when the destructor's implicit `request_stop()` fires. No code calls `notify_all()` for shutdown.

## Pattern: Monitor object

### The problem

During intake, several scan threads add items to and remove items from the same package manifest. A class with a public mutex, or none, leaves locking to every call site, and one missed `lock_guard` corrupts the state under load.

### The solution

A **monitor object** owns its mutex and takes it at the top of every public method, so callers never see or name the lock.

### How C++ models it

`ShipmentManifestMonitor` locks in each method body:

```cpp
void add_item(std::string sku, double weight_kg) {
    std::lock_guard lock(mutex_);
    items_.emplace_back(std::move(sku), weight_kg);
}

bool remove_item(const std::string& sku) {
    std::lock_guard lock(mutex_);
    // ...
}
```

Eight threads adding 200 items each leave exactly 1,600 items, every run. Without the lock, concurrent `emplace_back` calls on the same `std::vector` race and corrupt the heap.

A monitor object still runs each call on the caller's thread, one caller at a time. It serializes access; it doesn't move work anywhere.

## Pattern: Active object

### The problem

A label printer prints one job at a time. Callers want to say "print this" and keep going, not block until the printer finishes, and the printer must never receive two jobs at once.

### The solution

An **active object** turns each method call into a request on a private queue, served in order by its own thread, and returns a future for the result.

### How C++ models it

`LabelPrinterActiveObject::print_label()` builds a `std::packaged_task`, queues it and returns its future without running anything:

```cpp
std::future<PrintResult> print_label(ShippingLabelRequest request) {
    std::packaged_task<PrintResult()> task([request = std::move(request)] {
        return PrintResult{.label = render_label(request),
                            .executed_on = std::this_thread::get_id()};
    });
    std::future<PrintResult> future = task.get_future();
    {
        std::lock_guard lock(mutex_);
        requests_.push_back(std::move(task));
    }
    cv_.notify_one();
    return future;
}
```

One `std::jthread`, started in the constructor, runs the requests in submission order. `print_label()` returns in under 50 ms however long the render takes, and `PrintResult::executed_on` differs from the caller's thread id.

### Thread pool or active object

Every request has the same signature, so `requests_` is a plain `std::deque<std::packaged_task<PrintResult()>>`: no type erasure and no C++20/C++23 split.

| | Thread pool | Active object |
| --- | --- | --- |
| Work accepted | Many unrelated jobs | One narrow request type |
| Workers | Several, sharing one queue | One dedicated thread |
| Order | Any | Submission order |
| Queue element | Type-erased `Task` | `std::packaged_task<PrintResult()>` |

{% include excalidraw.html name="24-threadpool-vs-active-object" caption="Figure 24.2: the thread pool's shared queue serving concurrent workers against the active object's private queue serving one dedicated worker in order." %}

<blockquote class="ai-sidebar" markdown="1">
**Specifying a connection pool.** "Write a thread-safe connection pool" doesn't say how a connection comes back, and an `acquire()` that returns a raw pointer plus a `release(connection)` method leaks a connection on every error path that skips the call.

> In `examples/24-resource-pools/src/resource_pool.hpp`, make `ResourcePool<Resource>::acquire(std::chrono::milliseconds)` return a move-only `Lease` whose destructor is the only path back to the free list, with no public release method. Wait with a `std::condition_variable` predicate under `std::unique_lock`, never a sleep loop, and throw `std::runtime_error` on timeout or after `shutdown()`. Build connections only through the `ConnectionFactory` passed to `TrackingStoreConnectionPool`, and never more than the capacity.

Check: `ctest --preset cpp23 -R 'ResourcePoolTest|TrackingStoreConnectionPoolTest'` passes, and the reply has no `release(` method callers can reach.
</blockquote>

## When not to use it

| Pattern | Skip it when | Instead |
| --- | --- | --- |
| Resource pool | The resource is cheap to build, and construction never shows in a profile | Construct directly |
| Thread pool | The work is I/O-bound and mostly waits | Coroutines or async I/O; blocked threads are an expensive way to wait |
| Thread pool sized past the core count | The work is CPU-bound | Size to the cores; extra threads add only context switches |
| Monitor object | One thread ever touches the object | A plain class; every lock is waste |
| Monitor object | The need is an ordered queue of operations, not shared fields | An active object |
| Active object | Tasks have no ordering requirement, or the caller can block | A thread pool, or a direct synchronous call |

## Build, run, observe

From the repository root, the superbuild picks up `examples/24-resource-pools`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 19 cases pass under `cpp23` and 18 under `cpp20`; the `std::expected` case compiles only under C++23. The demo runs all four shapes:

```sh
examples/24-resource-pools/demo.sh cpp23
```

```
== Connection pool: bounded, blocking, reused ==
Leased connections 1 and 2; pool created=2 of capacity=2
try_acquire while exhausted: pool exhausted: every resource is currently leased
After releasing one lease, acquire() reused connection 1 (same id as before: yes); pool still created only 2 connection(s) total

== Thread pool: workers draining a shipping-label task queue ==
Rendered label for SHM-2001
Rendered label for SHM-2002
Rendered label for SHM-2003
Rendered label for SHM-2004
Rendered label for SHM-2005
All 5 submitted labels rendered by 3 worker thread(s)

== Monitor object: ShipmentManifestMonitor serializes concurrent scans ==
4 concurrent scanners each added 25 items; manifest now holds 100 items, total weight 100 kg

== Active object: LabelPrinterActiveObject decouples call from execution ==
Printed SHM-4001 on a different thread
Printed SHM-4002 on a different thread
Printed SHM-4003 on a different thread
```

Under `cpp20` the `try_acquire` line reads "failed (no reason available)"; every other line is identical.

Two experiments show what each guard buys:

- **Empty the lease destructor.** In a scratch copy, make `~Lease()` an empty body. Three cases fail: leases no longer return their resource, and the pool calls `create()` more than twice.
- **Drop the monitor's lock.** In a scratch copy, delete the `std::lock_guard` in `ShipmentManifestMonitor::add_item()`. The eight-thread intake case aborts or segfaults on every run as the `std::vector` is corrupted.

## For deeper coverage

- Douglas C. Schmidt, Michael Stal, Hans Rohnert, and Frank Buschmann, [*Pattern-Oriented Software Architecture, Volume 2: Patterns for Concurrent and Networked Objects*](https://www.wiley.com/en-us/Pattern-Oriented+Software+Architecture,+Volume+2,+Patterns+for+Concurrent+and+Networked+Objects-p-9781118725177) (Wiley, 2000): the original entries for Monitor Object and Active Object, plus the half-sync/half-async reasoning behind a thread pool's queue.
- Anthony Williams, [*C++ Concurrency in Action*](https://www.manning.com/books/c-plus-plus-concurrency-in-action-second-edition), Second Edition (Manning, 2019): the mechanics behind `std::mutex`, `std::condition_variable`, `std::jthread` and the memory model. The sibling site [*C++ Concurrency and Parallelism*](https://patterncatalyst.github.io/cpp-concurrency-and-parallelism/) builds on the same ground.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/24-resource-pools`, which checks concurrent behavior under bounded waits rather than sleeps.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).

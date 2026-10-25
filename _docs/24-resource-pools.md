---
title: "Resource pools"
order: 24
part: "patterns-in-the-wild"
description: "A generic, bounded resource pool with an RAII lease handle, taught against a shipment-tracking connection pool, plus four sibling concurrency shapes from the same domain: a thread pool rendering shipping labels, a monitor object, and an active object."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Build a generic `ResourcePool<Resource>` that bounds how many expensive
  resources exist at once, and a `PoolLease<Resource>` RAII handle that
  returns its resource to the pool automatically, on every exit path.
- Build a connection pool, the canonical instance of that generic pool,
  with a blocking `acquire()` that honors a timeout and a thread-safe
  implementation built on a mutex and a condition variable.
- Build a thread pool whose workers drain a shared task queue, and explain
  why a C++23 `std::move_only_function` removes an allocation a C++20
  `std::function` queue cannot avoid.
- Build a monitor object, where an object owns its own mutex and every
  public method is serialized, and tell it apart from a plain class with a
  mutex member that callers forget to lock consistently.
- Build an active object, where invoking a method only enqueues a request
  and returns a `std::future`, decoupling the call from whatever thread
  eventually executes it.
- Say which of these five shapes to reach for given a concrete scenario,
  and where the deep threading mechanics behind all of them live when this
  chapter's pattern-shape view is not enough.

{% include excalidraw.html name="24-resource-pools" caption="Figure 24.1: the generic ResourcePool and PoolLease, the TrackingStoreConnectionPool built on them, and three related concurrency shapes, a thread pool, a monitor object, and an active object, that share the same bounded-resource and serialized-access concerns." %}

## Why this matters

A shipment-tracking service does not open a fresh connection to its
tracking store for every incoming scan event. Opening that connection
costs a DNS lookup, a TCP handshake, and often a TLS negotiation and an
authentication round trip, work measured in milliseconds against a query
that itself takes microseconds once the connection exists. Doing that
setup on every request would mean the connection costs more than the work
it enables, every single time. The fix practitioners reach for is older
than this book's other chapters: build a bounded set of connections once,
hand them out to whoever needs one, and take them back when the caller is
done. That is a resource pool, and despite being one of the most-used
patterns in production C++, it never made Gamma, Helm, Johnson, and
Vlissides' original 1994 catalogue; it was formalized later, most notably
in Schmidt, Stal, Rohnert, and Buschmann's *Pattern-Oriented Software
Architecture, Volume 2*, the same volume that catalogues two of this
chapter's other patterns, Monitor Object and Active Object.

This chapter treats the resource pool as a generic mechanism first, then
builds a connection pool as the mechanism's canonical instance, and then
walks through three sibling shapes practitioners reach for under the same
production pressure: a thread pool for CPU-bound work with a natural
queue, a monitor object for state several threads touch directly, and an
active object for one narrow request type that needs its own serialized
timeline. All five patterns share a concern, bounding or
serializing access to something that is expensive or unsafe to share
carelessly, which is why they belong in one chapter even though a
connection pool and an active object look nothing alike on the page.

What this chapter does not do is re-teach `std::mutex`,
`std::condition_variable`, `std::jthread`, or the C++ memory model from
first principles. The sibling book in this series,
[*C++ Concurrency and Parallelism*](https://patterncatalyst.github.io/cpp-concurrency-and-parallelism/), covers that ground in the depth it
deserves: lock-free structures, memory ordering, false sharing, and the
reasoning behind every synchronization primitive used below. This chapter
assumes that mechanical knowledge and spends its words on the *shape* each
pattern gives that mechanism: what a pool, a monitor, and an active object
look like as types a caller depends on, and when each shape is the right
answer.

## The domain: a shipment-tracking service under load

Every example in this chapter lives in `examples/24-resource-pools` and
shares one domain: a service that tracks shipments as they move through a
carrier network. Three kinds of expensive-or-shared work recur across that
service, and each gets its own pattern:

- Recording a tracking event (a shipment left a facility, arrived at a
  hub, was delivered) means talking to a tracking store over a connection
  that is expensive to open and cheap to reuse. That is the connection
  pool's job.
- Rendering a shipping label is CPU-bound work, formatting text and
  computing a barcode payload, that several requests want done
  concurrently without one request waiting behind another on a single
  thread. That is the thread pool's job.
- A shipment's manifest, the list of line items inside one package, gets
  touched by several warehouse-scan threads as items are added or removed
  during intake. That shared, mutable state is the monitor object's job.
- Printing a label on a physical printer is inherently one-at-a-time work
  with its own timeline; callers want to fire off a print request and keep
  going, not block until the printer finishes. That is the active
  object's job.

## The generic resource pool and its RAII lease

The centerpiece is `ResourcePool<Resource>` in
`src/resource_pool.hpp`. It holds a bounded free list of
`std::unique_ptr<Resource>`, builds new resources lazily up to a capacity,
and hands them out through a nested `Lease` type:

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

`acquire()` blocks under a `std::unique_lock` and a `std::condition_variable`
until one of three things is true: a resource is already free, the pool
has not yet built as many resources as its capacity allows, or the
timeout elapses. The predicate form of `condition_variable::wait_for`
does the waiting; this chapter leans on that primitive without re-deriving
why it is immune to the lost-wakeup and spurious-wakeup problems a
hand-rolled flag-and-sleep loop is not, because *C++ Concurrency and Parallelism*
already covers that proof in full.

`Lease` is the other half of the idiom, and the reason this pattern is
safe to use at all under exceptions. A `Lease` is move-only: copying a
handle to a pooled resource would let two callers believe they each
exclusively own the same connection, so the copy constructor and copy
assignment are deleted. Its destructor calls back into the pool and pushes
the resource onto the free list, whether the scope that held the `Lease`
exited normally, early via a `return`, or by exception unwinding past it.
A caller never writes `pool.release(connection)`; forgetting to call a
release method is a whole category of bug this design makes impossible to
write; there is no release method to forget.

```cpp
{
    auto lease = pool.acquire(100ms);
    lease->record_event("SHM-1001", "in_transit");
}  // lease destructor runs here: the connection returns to the pool
```

This is the same RAII discipline that governs memory and file
handles, applied to something with a bound on how many can exist at once.
The bound is what makes a pool more than a cache: a cache grows to fit
demand, while a pool's whole purpose is to cap demand at a number the
backing resource (a connection limit, a thread count, a licensed
hardware seat) can actually sustain.

## The connection pool: the canonical instance

`TrackingStoreConnectionPool` in
`src/tracking_store_connection_pool.hpp` wraps
`ResourcePool<TrackingStoreConnection>` and nothing else. It exists so
call sites depend on a named, domain-shaped type, "a pool of tracking-store
connections," instead of a bare template instantiation, and so the seam
for testing has one obvious home:

```cpp
class ConnectionFactory {
public:
    virtual ~ConnectionFactory() = default;
    virtual std::unique_ptr<TrackingStoreConnection> create() = 0;
};
```

`TrackingStoreConnectionPool`'s constructor takes a
`std::shared_ptr<ConnectionFactory>` rather than building connections
itself. That seam is what makes the pool's central claim testable: a test
substitutes a `MockConnectionFactory` and asserts, via GoogleMock's
`EXPECT_CALL(*factory, create()).Times(2)`, that ten acquire-and-release
cycles against a capacity-2 pool call `create()` exactly twice, never once
per acquire. Without that mock, a test could only observe that the demo
runs and prints plausible numbers; with it, the test fails loudly the
moment a change accidentally makes the pool build a fresh connection on
every request, which is exactly the regression a connection pool exists
to prevent.

`TrackingStoreConnectionPool` forwards `acquire()`, `try_acquire()`, and
`shutdown()` straight through to the `ResourcePool` underneath it. That
thin forwarding is deliberate: the pattern's value is entirely in the
generic mechanism, and a canonical instance should not re-implement
anything the mechanism already got right.

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

`try_acquire()` is the non-blocking sibling of `acquire()`: it never
waits, it either hands back a `Lease` immediately or reports failure.
Under C++20 that failure collapses "every connection is leased" and "the
pool was shut down" into the same empty `std::optional`, mirroring the
exact narrowing a bare `std::optional` forces on any failed lookup. Under
C++23, `PoolAcquireError` distinguishes `Exhausted` from `Closed`, and
`tests/resource_pool_test.cpp`'s `TryAcquireReportsWhyItFailedUnderCpp23`
case proves both values are reachable and distinct, guarded behind
`#if __cplusplus >= 202302L` so the mandatory C++20 build path never needs
`<expected>`.

## The thread pool: workers draining a shared queue

`LabelRenderThreadPool` in `src/label_render_thread_pool.hpp` is the shape
most engineers picture first when someone says "thread pool": a fixed set
of worker threads, started once, pulling callables off a shared queue
until told to stop. `submit()` wraps the caller's work in a
`std::packaged_task`, pushes it onto the queue, and returns the matching
`std::future` immediately, before any worker has necessarily picked the
task up:

```cpp
std::future<RenderedLabel> submit(ShippingLabelRequest request);
```

The queue itself needs a type-erased callable, because a thread pool's
whole point is accepting arbitrary, unrelated work, not one fixed
function signature. That type erasure is exactly where C++20 and C++23
diverge in a way worth stopping on, because `std::packaged_task` is
move-only and the type the queue stores has to account for that.

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

This is not a cosmetic difference. Every production thread pool written
against C++20 or earlier either pays that `shared_ptr` tax on every
submitted task or hand-rolls its own move-only type eraser, which is more
code solving a problem the standard library solved in C++23. `std::move_only_function`
exists specifically to remove this workaround, and
`examples/24-resource-pools` is built once under each standard precisely
so a reader can see both versions compile and both pass the identical
`AllSubmittedTasksRunExactlyOnce` test, proving the two implementations
are behaviorally equivalent and differ only in that one allocation.

`worker_loop()` is where `std::jthread` and `std::stop_token` do their
job without any hand-kept "please stop" flag:

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

`condition_variable_any::wait()`'s stop-token overload registers a
`std::stop_callback` that wakes every waiting worker the instant the pool
destructor's implicit `request_stop()` fires, so no code here calls
`notify_all()` by hand. Precisely how that callback wiring works, and what
guarantees it makes about memory visibility, is again
*C++ Concurrency and Parallelism*'s territory; this chapter only needs
that the mechanism exists and that reaching for it beats a polling loop
with a sleep.

## The monitor object: an object that serializes itself

`ShipmentManifestMonitor` in `src/shipment_manifest_monitor.hpp` answers a
different question than the pool and the thread pool: not "how many of
these may exist," but "how do several threads touch the same mutable
state safely." A monitor object's defining trait is that it owns its own
mutex and takes it at the top of every public method, so a caller never
sees the mutex, never names it, and cannot forget to lock it before
touching the object's state:

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

Contrast this with a plain class that exposes its mutex, or exposes
nothing but trusts every caller to wrap every call site in a lock. Both of
those designs push the locking discipline out to every call site, where
one missed `lock_guard` corrupts shared state in a way that only shows up
under load, days after the code that introduced the bug shipped. The
monitor object collapses that discipline into one place: the method body.
`tests/shipment_manifest_monitor_test.cpp`'s
`ConcurrentAddItemCallsAreAllSerializedWithoutLoss` test spins up eight
threads each adding two hundred items to the same manifest and asserts the
final count is exactly sixteen hundred, every run; without the lock inside
`add_item()`, that count would be unpredictable and usually short, a
classic torn-write race on `std::vector::emplace_back`.

A monitor object still runs every call on the *caller's* thread, one
caller at a time. That is the crucial distinction from the pattern below:
a monitor object serializes access, it does not move the work anywhere.

## The active object: decoupling invocation from execution

`LabelPrinterActiveObject` in `src/label_printer_active_object.hpp`
answers a third question: what happens when a caller wants to say "print
this" and move on immediately, while the actual printing happens on its
own timeline, one request after another, on a thread the caller never
touches. `print_label()` builds a `std::packaged_task<PrintResult()>`,
pushes it onto a private request queue, and returns the matching
`std::future` without running anything:

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

A single dedicated `std::jthread`, started once in the constructor, pulls
requests off that queue in submission order and runs them one at a time.
`tests/label_printer_active_object_test.cpp`'s
`SubmittingDoesNotBlockTheCallingThread` test asserts `print_label()`
itself returns in under fifty milliseconds regardless of how long the
eventual render takes, and `PrintLabelRunsOnADifferentThreadThanTheCaller`
compares `PrintResult::executed_on` against the calling thread's id to
prove the work really did move, not merely that the API looks
asynchronous.

The active object's queue skips something the thread pool's queue cannot:
type erasure. Every request an active object accepts has the exact same
signature, `print_label(ShippingLabelRequest) -> std::future<PrintResult>`,
so `requests_` is a plain `std::deque<std::packaged_task<PrintResult()>>`,
no `Task` wrapper, no `std::move_only_function`, no C++20/23 fork at all.
That contrast is the clearest way to tell the two patterns apart on sight:
reach for a thread pool when many different, unrelated jobs need to share
a pool of workers, and reach for an active object when one narrow request
type needs its own private, serialized timeline and a caller who never
blocks on submission.

> **AI sidebar.** Ask an LLM pairing partner for a "thread-safe connection
> pool" and a frequent result returns a raw pointer or reference from
> `acquire()` with a separate `release(connection)` method the caller must
> remember to call on every exit path, including exception paths the
> generated code rarely considers. That shape reintroduces exactly the bug
> class RAII exists to close: one forgotten `release()` in an error branch
> leaks a connection out of the pool forever, and the pool silently shrinks
> at runtime until no connection is left to acquire. Read that result
> against this chapter's `Lease`: a pooled resource's handle must be a
> move-only RAII type whose destructor is the only path back into the free
> list, with no parallel "give it back" method for a caller to forget. The
> same generated code often gets the thread pool wrong in a related way,
> storing tasks in a `std::vector` and locking around a manual index
> instead of a queue plus a condition variable, which works under a quick
> manual test and then busy-spins or misses wakeups the moment two workers
> contend for the same slot. Check whether a generated pool's acquire path
> returns an RAII handle and whether its worker loop blocks on a condition
> variable instead of polling; if either answer is no, the shape is wrong
> even when the code compiles and runs once.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan
toolchain from the prerequisites; there is no container anywhere in this loop.
From the repository root, the root superbuild picks up
`examples/24-resource-pools` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest
cases through `gtest_discover_tests`: 19 cases under the `cpp23` preset, 18
under `cpp20`, the difference being the one `std::expected`-specific case
that only compiles under C++23. Observed locally on Fedora 44 / clang
22.1.8:

```
100% tests passed, 0 tests failed out of 19   # cpp23
100% tests passed, 0 tests failed out of 18   # cpp20
```

To see all five patterns run end to end against real output rather than
an assertion that says they work, run the example's own demo script:

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

Running `examples/24-resource-pools/demo.sh cpp20` prints the same output
except the `try_acquire` line, which reads "failed (no reason available)"
instead of naming the exhaustion reason, since `PoolAcquireError` and
`describe()` only compile under the `cpp23` preset. Every other line is
identical, because the generic pool, the thread pool's task-execution
guarantees, the monitor object, and the active object all behave
identically under both standards; only the one lookup's failure reporting
and the task queue's internal storage moved forward.

## When not to use it

A resource pool pays for its added complexity when building the resource
is expensive relative to using it, and when demand is bursty enough that
reuse saves work a cache of size one could not. A `TrackingStoreConnection`
clears that bar easily; a cheap-to-construct value type never should be
pooled, because the pool's bookkeeping, the mutex, the condition variable,
the free list, costs more than just building a fresh instance every time.
If nothing about constructing the resource shows up in a profile, skip the
pool and construct directly.

A thread pool is worth building when the work is CPU-bound and plentiful
enough that several pieces of it need to run concurrently; for
I/O-bound work that spends most of its time waiting on a network call, an
async/await or coroutine-based design usually beats a thread pool outright,
because threads sitting blocked on I/O are a more expensive way to wait
than a suspended coroutine. A pool sized larger than the machine's core
count for CPU-bound work buys nothing but context-switch overhead.

A monitor object is the right choice when several threads need to call the
*same* object's methods directly and concurrently. It is the wrong choice
when only one thread ever touches the object, the lock-guard overhead on
every call is pure waste there, and it is also the wrong choice when the
real need is a serialized queue of distinct operations rather than
protected access to shared fields, which is what an active object is for.

An active object is the right choice when a caller must never block on
submission and when the receiving side needs one serialized timeline, a
printer, a single outbound connection, a hardware device that cannot
accept concurrent commands. It is overkill for work that could just run
on a thread pool with no ordering requirement between tasks, and it adds
a queue, a future, and a dedicated thread's lifetime to manage for what a
synchronous call would have done in one line if blocking the caller were
acceptable. Across all five shapes, the test that justifies the pattern is
the same one the forces framing teaches: name the plain alternative, a direct
constructor call, a single worker thread, an ordinary method call, next to
the pattern, and let a real bound, a real measured cost, or a real
concurrent-access bug decide between them.

## For deeper coverage

- Douglas C. Schmidt, Michael Stal, Hans Rohnert, and Frank Buschmann,
  [*Pattern-Oriented Software Architecture, Volume 2: Patterns for Concurrent and Networked Objects*](https://www.wiley.com/en-us/Pattern-Oriented+Software+Architecture,+Volume+2,+Patterns+for+Concurrent+and+Networked+Objects-p-9781118725177) (Wiley, 2000): the original catalogue
  entries for Monitor Object and Active Object this chapter teaches as
  idioms, plus the half-sync/half-async reasoning behind a thread pool's
  queue.
- Anthony Williams, [*C++ Concurrency in Action*](https://www.manning.com/books/c-plus-plus-concurrency-in-action-second-edition), Second Edition (Manning,
  2019): the deep mechanics behind every primitive this chapter uses
  without re-deriving, `std::mutex`, `std::condition_variable`,
  `std::jthread`, and the memory model underneath them. This book's
  sibling site, [*C++ Concurrency and Parallelism*](https://patterncatalyst.github.io/cpp-concurrency-and-parallelism/), builds on the same ground.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official
  [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind
  `examples/24-resource-pools`'s suite, which asserts concurrent behavior
  under bounded waits rather than sleeping and hoping.
- The full reading list behind this book's opinions lives on the
  [bibliography page]({{ '/bibliography/' | relative_url }}); each
  citation there is a pointer to the full book, not a substitute for
  reading it.

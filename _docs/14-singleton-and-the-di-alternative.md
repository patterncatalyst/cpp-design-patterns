---
title: "Singleton, and the dependency-injection alternative"
order: 14
part: "creating-objects-under-control"
description: "The thread-safe Meyers singleton, the real costs it hides behind a convenient accessor, and the constructor-injected abstraction that gives the same capability without the global state, wired by a framework-free composition root."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Build a thread-safe Meyers singleton with a function-local static, and say exactly which guarantee the language gives with no extra code.
- Name the costs a singleton imposes: global mutable state, a dependency no signature announces, no seam for a test double, and destruction-order risk between singletons.
- Rebuild the same capability as an abstraction injected through a constructor and wired once by a composition root.
- Test the injected version with a fresh double per test, and see why the singleton needs a test-only reset to do the same.
- Judge when a singleton is still the defensible choice.

## Why this matters

A shipment audit trail records every fulfillment event, warehouse packing, billing charge, carrier handoff, from whichever module triggers it. The convenient shape is a type with one instance that any code can reach without passing a reference around. That convenience is the Singleton intent, and it's the reason so many C++ codebases later wish they hadn't used one.

{% include excalidraw.html name="14-singleton-vs-di" caption="Figure 14.1: a singleton's clients reach into one global instance directly; the DI alternative routes the same capability through an abstraction a composition root wires, swappable with a test double." %}

Both versions below record the same message, built by one shared function, so every difference between them is in how the dependency is reached, not in what's logged:

```cpp
std::string format_fulfillment_event(const std::string& order_id) {
    return "Order " + order_id + " fulfilled";
}
```

## Pattern: Singleton

### The problem

A warehouse module and a billing module both write to the audit log, and neither should construct its own. Two logs would split the trail, and every module that needs the log would otherwise have to receive it from somewhere.

### The solution

A **Singleton** ensures a class has only one instance and provides a global point of access to it.

### How C++ models it

A Meyers singleton, after Scott Meyers's formulation, returns a reference to a function-local `static`:

```cpp
class ShipmentAuditLog {
public:
    static ShipmentAuditLog& instance();

    ShipmentAuditLog(const ShipmentAuditLog&) = delete;
    ShipmentAuditLog& operator=(const ShipmentAuditLog&) = delete;

    void record(std::string event);
    std::vector<std::string> entries() const;

private:
    ShipmentAuditLog() = default;

    mutable std::mutex mutex_;
    std::vector<std::string> entries_;
};

ShipmentAuditLog& ShipmentAuditLog::instance() {
    static ShipmentAuditLog log;
    return log;
}
```

Initialization of a function-local `static` is thread-safe by language rule: if two threads call `instance()` before `log` exists, exactly one runs the constructor and the other waits. No `std::call_once` or double-checked locking is needed. The guarantee covers construction only. Two threads calling `record()` on the finished object still race, which is why `ShipmentAuditLog` carries its own `mutex_`.

The private constructor stops anyone building a second `ShipmentAuditLog` directly. The deleted copy operations stop `ShipmentAuditLog copy = ShipmentAuditLog::instance();`, which would otherwise create a second, divergent log; it fails with `call to deleted constructor of 'ShipmentAuditLog'`. Those two rules are the mechanism.

### What the singleton costs

| Cost | What it looks like here |
| --- | --- |
| Global mutable state | `entries_` lives for the whole process, so a check that records and then reads the log sees whatever earlier code left behind |
| Hidden dependency | `record_from_warehouse_module(const std::string& order_id)` uses the log, and nothing in its signature says so |
| No seam for a double | There's one implementation, linked into every binary, and no parameter to swap a failing store in |
| Destruction order | Singletons that use each other in destructors can touch an already-destroyed object after `main()` returns |

Isolating a test from global state takes a method added for that purpose alone:

```cpp
void ShipmentAuditLog::reset_for_testing();
```

A method whose only job is to make global state look untouched sits in the production class's public interface as a record that the design needed an escape hatch to be tested. Without it, a check that runs twice in one process sees the first run's entries.

To test a backing store that fails, a full disk or a timed-out network logger, the singleton offers nowhere to put the failure. Reproducing it means rebuilding `ShipmentAuditLog` with faulty internals, which modifies the only production implementation rather than testing it.

The function-local `static` avoids the static initialization order problem of file-scope globals, because it's built on first use. Destruction is the remaining risk. Function-local statics are destroyed in reverse order of construction, and construction order between two singletons is whatever order first use happened in. A `MetricsRegistry` singleton that writes a final summary to the audit log from its destructor uses a destroyed object after `main()` returns if the audit log was constructed later and so destroyed first. A composition root's local variables have a visible order instead: they're destroyed in reverse of their declaration, on the page.

## Pattern: Dependency injection

### The problem

The same capability, one audit log reached by every part of the fulfillment pipeline, without a global accessor, a hidden dependency or a test-only reset.

### The solution

**Dependency injection** passes a component the abstractions it depends on, usually through its constructor, and leaves one composition root to build the concrete objects and wire them together.

### How C++ models it

`AuditSink` is the capability as an abstraction:

```cpp
class AuditSink {
public:
    virtual ~AuditSink() = default;
    virtual bool record(const std::string& event) = 0;
};
```

`InMemoryAuditSink` is the production implementation. `OrderFulfillmentService` depends on `AuditSink`, not on `InMemoryAuditSink`:

```cpp
class OrderFulfillmentService {
public:
    explicit OrderFulfillmentService(AuditSink& sink) : sink_(sink) {}

    FulfillmentReceipt fulfill_order(const std::string& order_id) const;

private:
    AuditSink& sink_;
};
```

The sink is held by reference because the service never reassigns it. Ownership belongs to the composition root, `demo_main.cpp`, which constructs one `InMemoryAuditSink` and passes it to every service that needs it. This is the same manual DI as the dependency-inversion chapter, with no framework, applied to a different capability. The constructor `explicit OrderFulfillmentService(AuditSink& sink)` states the one dependency where a reader looks first.

Singleton and DI answer the same question: how does code far from where a shared capability is built get access to it? DI costs an interface and a constructor parameter, and pays back in testability and visible wiring.

### Testing through the seam

Each test builds its own `FakeAuditSink`, hands it to a fresh `OrderFulfillmentService` and checks what that one sink captured. Fulfilling `"ORD-1"` records exactly `"Order ORD-1 fulfilled"`. There's no reset call, because there's no shared state. Two service-and-sink pairs never see each other's entries, a claim the singleton can't make about its one log. A `FailingAuditSink`, which rejects every write, exercises the failure path the singleton had no seam for. Substituting a double is one constructor call: `OrderFulfillmentService service(sink);`.

### Reporting a failed write

`try_fulfill_order()` reports a sink failure instead of returning a receipt that looks like success:

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: optional loses the reason for rejection; the caller learns
// *that* the sink rejected the write, not *why*.
std::optional<FulfillmentReceipt> try_fulfill_order(const std::string& order_id) const {
    const std::string message = format_fulfillment_event(order_id);
    if (!sink_.record(message)) {
        return std::nullopt;
    }
    return FulfillmentReceipt{order_id, message};
}
```
```cpp
// C++23: expected carries the rejection reason alongside the success
// value, checkable with an if and no exception machinery.
std::expected<FulfillmentReceipt, std::string> try_fulfill_order(const std::string& order_id) const {
    const std::string message = format_fulfillment_event(order_id);
    if (!sink_.record(message)) {
        return std::unexpected<std::string>("audit sink failed to record order " + order_id);
    }
    return FulfillmentReceipt{order_id, message};
}
```

Both columns depend on `AuditSink&` through the same constructor and composition root. Only the precision of the failure path changes.

## When not to use it

- **A singleton for mutable, shared state.** The audit log is changed by every call, read by audits and used by `OrderFulfillmentService`, warehouse modules and billing modules. That's the profile where a test needs a double and a reviewer needs the dependency in a signature, so inject it.
- **Dependency injection for something with nothing to substitute.** A process-wide resource with no plausible second implementation and nothing a test would fake, such as an output handle to a fixed file descriptor or a read-only table of physical constants loaded once at startup, costs close to nothing as a singleton. If exactly one instance must exist, it's effectively immutable after construction and no caller will substitute another, the singleton's convenience is real.

<blockquote class="ai-sidebar" markdown="1">
**Specifying a shared capability.** "Add a metrics registry the whole pipeline can use" leaves open whether it's a global accessor or an injected dependency, and how a test substitutes it. State it:

> In `examples/14-singleton-and-di`, add a `MetricsSink` interface with one `increment(const std::string& counter)` method and an `InMemoryMetricsSink` implementation. Inject it into `OrderFulfillmentService` through the constructor as a reference, next to `AuditSink&`, and wire one instance in the composition root in `demo_main.cpp`. Don't add an `instance()` accessor, a function-local static or a `reset_for_testing()` method. Add tests that each build their own fake `MetricsSink` and check that `fulfill_order()` increments `"orders_fulfilled"` once per call.

Run `ctest --preset cpp23 -R OrderFulfillment` and check that no new `static` appeared in `src/`.
</blockquote>

## Build, run, observe

From the repository root, the superbuild picks up `examples/14-singleton-and-di`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 9 cases pass under both presets; the two `try_fulfill_order()` cases compile against `std::expected` under C++23 and `std::optional` under C++20. To see both versions run:

```sh
examples/14-singleton-and-di/demo.sh cpp23
```

It records a warehouse event and a billing event into the one `ShipmentAuditLog`, confirms both call sites reached the same instance, records through the injected `InMemoryAuditSink`, and shows a typed rejection from a sink that refuses every write:

```
== Singleton: ShipmentAuditLog::instance() ==
[warehouse] packed ORD-2001
[billing] charged ORD-2001
Same instance from both call sites: yes

== Dependency injection: OrderFulfillmentService ==
Order ORD-2002 fulfilled
sink entry: Order ORD-2002 fulfilled

== C++23 refinement: typed failure via std::expected ==
Rejected: audit sink failed to record order ORD-2003
```

Under `cpp20` the output stops before the `std::expected` block.

- **Drop the reset.** In a scratch copy, delete the `reset_for_testing()` call at the top of the shared-log case, then run the GoogleTest binary with `--gtest_filter='*OneSharedLog' --gtest_repeat=2`. CTest still passes, because it runs each case in its own process. The repeated run fails on its second pass: the log holds 4 entries, not 2.
- **Copy the instance.** Write `ShipmentAuditLog copy = ShipmentAuditLog::instance();`. The build fails with `call to deleted constructor of 'ShipmentAuditLog'`.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Singleton entry, including the intent statement the Meyers implementation satisfies.
- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the Dependency Inversion Principle the `AuditSink` alternative applies.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).

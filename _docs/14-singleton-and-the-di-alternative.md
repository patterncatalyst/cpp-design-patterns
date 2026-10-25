---
title: "Singleton, and the dependency-injection alternative"
order: 14
part: "creating-objects-under-control"
description: "The thread-safe Meyers singleton, the real costs it hides behind a convenient accessor, and the constructor-injected abstraction that gives the same capability without the global state, with a direct cross-reference to Chapter 4's DI-without-a-framework composition root."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Build a thread-safe Meyers singleton with a function-local static, and explain exactly which guarantee C++11 onward gives you at no extra code.
- Name the concrete costs a singleton imposes: global mutable state, a hidden dependency no constructor signature announces, difficulty substituting a test double, and static-initialization-order risk across translation units.
- Rebuild the same capability as an abstraction injected by constructor, wired once by a composition root, following Chapter 4's manual-DI pattern rather than reinventing it.
- Prove the testability contrast directly: write a test that the singleton version structurally cannot offer without a test-only backdoor, and the equivalent test that the injected version offers with no backdoor at all.
- Judge when a singleton is still the defensible choice and when the DI alternative is worth the extra abstraction.

## Why this matters

A shipment audit trail has to record every fulfillment event, warehouse packing, billing charge, carrier handoff, from whichever module in the system triggers it. The convenient shape for that is a type with exactly one instance, reachable from anywhere in the codebase without threading a reference through every call site that needs to log something. That convenience is also the Gang of Four's Singleton intent, stated directly: ensure a class has only one instance and provide a global point of access to it. Every C++ programmer has reached for this pattern, and most C++ programmers have, at some point, regretted it.

This chapter teaches Singleton in full: the mechanism that makes a modern C++ singleton thread-safe, the real engineering costs that mechanism does not solve, and the dependency-injection alternative that gives an identical capability, one audit log, reached by every part of a fulfillment pipeline, without the costs. Chapter 4 already built the DI machinery this chapter needs: constructor injection with no framework, a composition root that wires concrete types to abstractions by hand, and the judgment call between owning a dependency through a smart pointer and merely borrowing it through a reference. This chapter does not repeat that mechanism. It applies it to a specific, concrete decision: is this shipment audit log going to be a globally reachable object, or an abstraction some composition root hands to the things that need it.

{% include excalidraw.html name="14-singleton-vs-di" caption="Figure 14.1: a singleton's clients reach into one global instance directly; the DI alternative routes the same capability through an abstraction a composition root wires, swappable with a test double." %}

## The domain: auditing a shipment fulfillment event

The example behind this chapter lives in `examples/14-singleton-and-di`. Both versions record the same fact, an order was fulfilled, through the same message text, built by one shared function so a wording fix lands in both versions at once, the same shared-core move Chapters 8 and 4 used for their own domains:

```cpp
std::string format_fulfillment_event(const std::string& order_id) {
    return "Order " + order_id + " fulfilled";
}
```

The singleton version is `ShipmentAuditLog`, reached through `ShipmentAuditLog::instance()`. The DI version is `AuditSink`, an interface with one concrete production implementation, `InMemoryAuditSink`, injected into a high-level policy called `OrderFulfillmentService`. Keeping the recorded text identical in both versions is what makes the chapter's claim checkable rather than asserted: the two versions solve the identical problem, so every difference that shows up between them is a difference in how the dependency is reached, not a difference in what gets logged.

## The Meyers singleton, done correctly

A Meyers singleton, named for Scott Meyers's widely cited formulation, returns a reference to a function-local `static` object:

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

This is the version of Singleton worth teaching, because the mechanism it relies on is a real, standard-guaranteed one rather than a hand-rolled race condition. Since C++11, initialization of a function-local `static` is thread-safe by language rule: if two threads call `instance()` concurrently before `log` has been constructed, the standard guarantees exactly one of them runs the constructor and the other blocks until it finishes, with no `std::call_once` or double-checked locking required in the code above `instance()` itself. That guarantee is specifically about *construction*; it says nothing about what happens when two threads call `record()` on the already-constructed object at the same time, which is why `ShipmentAuditLog` still carries its own `mutex_` to serialize access to `entries_`. A correct Meyers singleton solves exactly one problem, safe lazy construction, and still needs the same thread-safety discipline as any other shared mutable object for everything that happens after construction.

The copy constructor and copy-assignment operator are deleted, and the constructor is private; both lines are doing real work. Without the deleted copy operations, a caller could write `ShipmentAuditLog copy = ShipmentAuditLog::instance();` and end up with a second, divergent audit log holding its own `entries_`, which defeats the entire point of the pattern. Without the private constructor, nothing stops a second `ShipmentAuditLog` from being constructed directly, bypassing `instance()` altogether. These two rules are the mechanical definition of Singleton; everything else in this section is what that mechanism costs once it is used correctly.

## What the singleton costs, concretely

Four costs follow from this shape, and this example's own test suite makes each one visible rather than asserting it abstractly.

The first is global mutable state. `entries_` lives for the entire process, shared by every caller, with no scope that bounds its lifetime to a request, a test, or a unit of work. `examples/14-singleton-and-di/tests/shipment_audit_log_singleton_test.cpp` demonstrates the direct consequence: a test that calls `record()` and then asserts on `entries()` is not testing a fresh object. It is testing whatever `entries_` happens to contain, including whatever any earlier test, in this file or another one linked into the same binary, left behind. The only way to get an isolated test is to call a method whose entire purpose is working around this problem:

```cpp
void ShipmentAuditLog::reset_for_testing();
```

A method that exists purely to make the global state behave, for one test, as if it had never been touched, is not a minor inconvenience. It is evidence, written directly into the production class's public interface, that the design's global state needed a dedicated escape hatch before it could be tested at all. A type built around dependency injection, as this chapter's second half shows, never needs an equivalent method, because nothing about its design shares state across tests in the first place.

The second cost is a hidden dependency. Nothing in the signature of `record_from_warehouse_module(const std::string& order_id)` reveals that the function talks to `ShipmentAuditLog`. A reader has to open the function body to discover it, and a reviewer checking what a function depends on from its signature alone, the same check Chapter 4 trained for constructor parameters, gets no help at all from a singleton accessor. Compare that to `OrderFulfillmentService`'s constructor later in this chapter, `explicit OrderFulfillmentService(AuditSink& sink)`, which states its one dependency in the one place a reader is guaranteed to look.

The third cost is the one this chapter can prove directly rather than describe: substituting a test double is hard, not impossible, but hard in a way that leaves a permanent mark on the production class. `ShipmentAuditLog` has exactly one implementation, compiled into every binary that links it, and there is no parameter anywhere that lets a test swap in a different backing store. To test what happens when the audit log's backing store fails, a disk fills up, a network-backed logger times out, the singleton's design offers no seam at all; the behavior would have to be reproduced by rebuilding `ShipmentAuditLog` itself with faulty internals, which is not a test, it is a modification to production code's only implementation.

The fourth cost is static-initialization-order risk, and it is the one that does not show up in this chapter's own small example precisely because the example is too small to hit it. The function-local `static` inside `instance()` sidesteps the classic "static initialization order fiasco" that plagues file-scope globals, because it is constructed on first use rather than at some unspecified point before `main()`. The risk returns the moment a second singleton's constructor calls the first singleton's `instance()`: C++ guarantees each is safely constructed on its own first use, but gives no guarantee about which one is first if both are reachable from unrelated call sites during static destruction at program exit, where the order of destructor calls across independent function-local statics is the reverse of construction order, and construction order between *different* singletons is whatever order first use happened to occur in. A fulfillment pipeline with both a `ShipmentAuditLog` singleton and, say, a `MetricsRegistry` singleton that logs a final summary to the audit log from its own destructor can crash on shutdown, after `main()` returns, if `ShipmentAuditLog`'s static happened to be destroyed first. This class of bug is specific to singletons depending on other singletons; a composition root wiring two ordinary objects together has an explicit, visible order, because the composition root's own local variables are destroyed in the reverse of the order they were declared, a rule a reader can see on the page.

## The dependency-injection alternative

`AuditSink` models the identical capability as an abstraction:

```cpp
class AuditSink {
public:
    virtual ~AuditSink() = default;
    virtual bool record(const std::string& event) = 0;
};
```

`InMemoryAuditSink` is the one production implementation, and `OrderFulfillmentService` is the high-level policy that depends on `AuditSink`, not on `InMemoryAuditSink`, following the Dependency Inversion Principle from Chapter 4 exactly:

```cpp
class OrderFulfillmentService {
public:
    explicit OrderFulfillmentService(AuditSink& sink) : sink_(sink) {}

    FulfillmentReceipt fulfill_order(const std::string& order_id) const;

private:
    AuditSink& sink_;
};
```

The dependency is held as a reference, the same choice Chapter 4 made for `OrderNotificationPolicy`'s `MessageSender&`, and for the identical reason: `OrderFulfillmentService` never reassigns its sink, so ownership belongs to whichever composition root constructs the graph, not to the policy borrowing a role out of it. `demo_main.cpp` is that composition root, constructing `InMemoryAuditSink` exactly once and passing it by reference to every service that needs it. Nothing new is happening here; this is Chapter 4's manual-DI shape applied to a different capability, which is the point. Singleton and dependency injection are not two unrelated patterns in this book's catalogue. They are two answers to the same question, "how does code far from the object that constructs a shared capability get access to it," and DI's answer is cheaper every time the forces favor testability and explicit wiring over global reachability.

The payoff shows up directly in the test suite. `tests/order_fulfillment_service_test.cpp` constructs a `FakeAuditSink`, records nothing anywhere else, and asserts on exactly what that one `FakeAuditSink` captured:

```cpp
TEST(OrderFulfillmentServiceTest, FulfillOrderRecordsTheExactAuditMessage) {
    FakeAuditSink sink;
    OrderFulfillmentService service(sink);

    const FulfillmentReceipt receipt = service.fulfill_order("ORD-1");

    EXPECT_EQ(receipt.audit_message, "Order ORD-1 fulfilled");
    ASSERT_EQ(sink.recorded().size(), 1U);
}
```

No `reset_for_testing()` call anywhere, because there is no shared state to reset; `sink` is a fresh object this test alone owns. A second test constructs two independent `FakeAuditSink`/`OrderFulfillmentService` pairs and proves they never share state, which is not a claim the singleton version can make about its one shared `ShipmentAuditLog`, since there is only ever one of it. A third test substitutes `FailingAuditSink`, a double that rejects every write, to exercise `OrderFulfillmentService`'s failure path directly, the exact scenario the singleton's one hardwired implementation could not offer a seam for in the section above. The injected abstraction turns "hard to substitute a test double" from a described cost into a one-line constructor call: `OrderFulfillmentService service(sink);`, where `sink` is whichever implementation the test wants.

## Typed error handling: C++20 versus C++23

`OrderFulfillmentService::try_fulfill_order()` reports a sink failure explicitly instead of returning a receipt that looks the same as a success, following the same codetabs convention Chapters 4 and 8 both used for their own failure paths:

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

Both columns depend on exactly `AuditSink&`, through the same constructor, wired by the same composition root. Only the failure path's precision moved between standards; the decision this chapter trains, inject the capability instead of reaching into a global, is identical in both columns.

> **AI sidebar.** Ask an LLM pairing partner for "a singleton class for X" and it will usually hand back exactly the shape this chapter opened with, `instance()`, a private constructor, deleted copy operations, because that shape is common in training data and is, mechanically, not wrong. What a generated answer will not do on its own is ask whether a singleton is the right call for your X. Check the forces before accepting it: does anything in the codebase already need to substitute a fake for this capability in a test, or will it soon. Does more than one component need to depend on this capability independently, in a way a composition root could wire explicitly instead. If the answer to either is yes, the generated singleton is a maintenance cost with a convenient name, and the fix is the one this chapter just built: an interface, a composition root, a reference held by whoever needs the capability. A generated `instance()` accessor with every caller in the same translation unit and no test anywhere substituting a fake is the surest sign the model reached for the familiar shape instead of the one the forces call for.

## When a singleton is still the defensible choice

None of this makes Singleton wrong in every case. A process-wide, stateless resource with no plausible second implementation and no test that would benefit from faking it, a logger's output stream handle to a fixed file descriptor, a read-only table of physical constants loaded once from a configuration file that never changes after startup, is a case where the pattern's cost is close to zero, because there is nothing to substitute, nothing mutable to race on beyond what the type already protects, and no second instance any design would ever want. The question to ask is the one Chapter 1 trained for every pattern in this book: is there a force here that a singleton resolves, or is this a plain object away from being simpler. If the answer is that exactly one instance must exist, that instance is effectively immutable after construction, and no caller will ever need to substitute a different one, Singleton's cost shrinks close to zero and its convenience is real.

The shipment audit log in this chapter's own example is a case where the pattern does not clear that bar. It is mutated by every call, read by audits, and depended on by more than one component, `OrderFulfillmentService`, warehouse modules, billing modules, exactly the profile where a test needs to substitute a double and a reviewer needs to see the dependency in a signature. That is why this chapter built the DI alternative as the recommended shape for it, not as an exotic refinement, and why Chapter 4's constructor-injection machinery, not a bespoke pattern invented here, is what the alternative reuses.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/14-singleton-and-di` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 9 cases under both the `cpp23` and `cpp20` presets, the two `try_fulfill_order`-specific cases compiling against `std::expected` under one standard and `std::optional` under the other rather than being compiled out entirely. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 9   # cpp23
100% tests passed, 0 tests failed out of 9   # cpp20
```

To see the singleton and the injected version side by side rather than just reading an assertion that says they differ, run the example's own demo script:

```sh
examples/14-singleton-and-di/demo.sh cpp23
```

It prints two warehouse-and-billing events landing in the one shared `ShipmentAuditLog`, confirms both call sites reached the same instance, then prints the injected `OrderFulfillmentService` recording into its own `InMemoryAuditSink`, and finally a C++23 typed rejection from a sink that refuses every write:

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

Running `examples/14-singleton-and-di/demo.sh cpp20` prints the same output minus the final `std::expected` block, which only compiles under the `cpp23` preset.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Singleton entry, including the intent statement this chapter's Meyers implementation satisfies and the costs this chapter makes concrete.
- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the Dependency Inversion Principle this chapter's `AuditSink` alternative applies, covered in full in Chapter 4.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.

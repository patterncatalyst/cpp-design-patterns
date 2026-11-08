---
title: "The 24 dependency-breaking techniques for C++20/23"
order: 28
part: "changing-inherited-code"
description: "Michael Feathers' 24 dependency-breaking techniques mapped to modern C++, then applied to inherited shipping code: a mail dependency behind an interface and a defaulted constructor parameter, a clock read isolated into an overridable call, a mutable global behind an accessor, a static lookup behind an instance delegator, and a file-parsing parameter adapted down to the two values a caller reads."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Name Feathers' 24 dependency-breaking techniques and the modern C++ tool that expresses each one.
- Combine Extract Interface and Parameterize Constructor to open a seam without touching an existing call site.
- Isolate one troublesome expression with Extract and Override Call, as a virtual method or a deducing-this hook.
- Wrap a mutable global behind an interface with Encapsulate Global References, leaving the global where it is.
- Put an injectable object in front of a static-only API with Introduce Instance Delegator.
- Shrink a heavyweight parameter to the values a method reads with Adapt Parameter.
- Pick the technique that costs least for the seam a test needs.

{% include excalidraw.html name="28-dependency-breaking" caption="Figure 28.1: the same call site, two shapes. Before, ShipmentAlertNotifier constructs and calls SmtpAlertSender directly, with no seam. After Extract Interface and Parameterize Constructor, the notifier depends on an AlertChannel interface, and either a production SmtpAlertSender or a test double can sit behind it." %}

## Why this matters

The risk in inherited code is rarely the logic. It's what the logic is wired to: a mail client constructed in a constructor body, a `system_clock::now()` read in the middle of a method, a global an operations console sets at startup, a static utility called from forty places, a parameter type that opens a file when it's constructed. Each was reasonable when written. The cost arrives when a test of one method needs a real SMTP connection, the real clock or a real file.

Feathers' *Working Effectively with Legacy Code* doesn't offer one fix. It catalogues 24 techniques, each a small mechanical move that breaks one kind of dependency by opening one kind of **seam**, a place where behavior can change without editing the code there. Each answers the same question: what's the smallest change that lets a test substitute something for this dependency?

A global and a static-only utility are different problems, and "add dependency injection" as a blanket answer means a rewrite larger than either needs. Every piece of inherited code below keeps its behavior and its callers while gaining one seam.

## The full catalogue, mapped to modern C++20/23

Feathers wrote the catalogue when `virtual` and a hand-written interface were nearly the only seams. Modern C++ adds concepts for structural interfaces, `std::function` and `std::move_only_function` for a pluggable callable, and deducing this for static polymorphism that reads like an override without the virtual call. Entries marked ★ are demonstrated in `examples/28-dependency-breaking`.

| Technique | What it breaks | Modern C++20/23 expression |
|---|---|---|
| Adapt Parameter★ | A method depends on a heavyweight parameter it reads only a few values from | A minimal interface (or a concept) for the needed surface, plus an adapter |
| Break Out Method Object | A long method resists isolation | A small class with `operator()`, locals becoming members |
| Definition Completion | A declared symbol has no definition a test build can link | A test-only translation unit with the definition |
| Encapsulate Global References★ | Code reads or writes a global inline wherever it needs the value | Wrap the global behind an interface and inject the wrapper |
| Expose Static Method | Logic needing no instance state is trapped in an instance method | A `static` member or a free function in a detail namespace |
| Extract and Override Call★ | One troublesome call sits inline in an otherwise-fine method | Its own method, overridden virtually or hidden through deducing this |
| Extract and Override Factory Method | An embedded `new` hard-wires the concrete type | A factory method a test subclass overrides |
| Extract and Override Getter | A troublesome expression, such as a global read, is inlined at every use | A getter a test subclass overrides |
| Extract Implementer | A class mixes a contract worth keeping with an implementation that must vary | An abstract interface plus an implementer, or a `std::function` |
| Extract Interface★ | A class can only use one concrete collaborator | The collaborator's minimal contract as an abstract base |
| Introduce Instance Delegator★ | A static API has callers that can't all change at once | A thin instance class that implements an interface and forwards |
| Introduce Static Setter | A global must be substitutable by a test but has no write path | A static setter paired with an RAII guard that restores the value |
| Link Substitution | A dependency must be replaced at link time | A test-only translation unit with the same symbols |
| Parameterize Constructor★ | A constructor builds its own collaborator | A constructor parameter defaulted to the original construction |
| Parameterize Method | One method reaches for a hard-wired collaborator | Pass the collaborator as an argument |
| Primitivize Parameter | A method needs two primitives inside a hard-to-build object | Accept the primitives directly |
| Pull Up Feature | A feature a test needs lives on a subclass with unwanted dependencies | Move the feature to a shared base class |
| Push Down Dependency | A base class carries a dependency only some subclasses need | Move the dependency into those subclasses |
| Replace Function with Function Pointer | A C-style call can't be intercepted by an object seam | A `std::function` or `std::move_only_function` member |
| Replace Global Reference with Getter | A direct global read sits inline in a method body | Replace the read with a virtual (or injected) getter |
| Subclass and Override Method★ | A method's implementation must be suppressed or replaced for a test | A test-only subclass overriding it |
| Supersede Instance Variable | A collaborator is wired into a built object with no constructor seam yet | A setter, as a step toward a constructor seam |
| Template Redefinition | A class needs a different collaborator type for tests, resolved at compile time | Make the collaborator type a template parameter, optionally constrained by a concept |
| Text Redefinition | In interpreted languages, the source text is patched for a test build | In C++, a preprocessor seam, as a last resort |

Two entries need more than a row. Template Redefinition suits C++ better than the other languages Feathers wrote for: a class template parameterized on its collaborator gets a compile-time seam with no virtual call and no interface, at the cost of every client knowing the template parameter. Text Redefinition is a last resort even where it's easy, and a `#define` seam in C++ risks violating the one-definition rule across translation units.

## Extract Interface and Parameterize Constructor: opening a mouth for a seam

The inherited `ShipmentAlertNotifier` emails a warehouse supervisor when a shipment is flagged for manual review. Its constructor built a concrete `SmtpAlertSender` itself, with no interface. The author needed one way to send a message, and an interface for one implementation would have looked like speculative generality.

Extract Interface targets the dependency, not the dependent. `AlertChannel` pulls out the one method the notifier calls, `send(recipient, message)`, leaving the rest of `SmtpAlertSender`'s surface, such as connection pooling and retries in a real system, behind:

```cpp
class AlertChannel {
public:
    virtual ~AlertChannel() = default;
    virtual void send(const std::string& recipient, const std::string& message) const = 0;
};

class SmtpAlertSender : public AlertChannel {
public:
    explicit SmtpAlertSender(std::string smtp_host);
    void send(const std::string& recipient, const std::string& message) const override;
    // ...
};
```

The interface only makes a second implementation possible. Parameterize Constructor opens the seam, and the default argument makes it safe for inherited code:

```cpp
explicit ShipmentAlertNotifier(
    std::string supervisor_email,
    std::shared_ptr<AlertChannel> channel = std::make_shared<SmtpAlertSender>("mail.internal.example"));
```

Every existing call site, `ShipmentAlertNotifier notifier("ops@example.com");`, still compiles and behaves the same: the default constructs the same `SmtpAlertSender` the old constructor built. Only a test passes the second argument: a GoogleMock `MockAlertChannel` checked for the exact recipient and part of the message, or a recording fake that captures the formatted text. No socket opens.

## Extract and Override Call: isolating the one call that moves

`ShipmentEtaScheduler::schedule_shipment()` validates a shipment and computes a promised delivery date from a transit-day count. That logic is fine. The problem is one line in the middle: a call to `std::chrono::system_clock::now()` to stamp the dispatch time. Every run gets a different "now", so a test can only check that the result falls in a range, never that it equals one known answer.

Extract and Override Call fits one troublesome expression inside a method too small to restructure. Pull the call into its own method; production behavior doesn't change:

{% include codetabs.html langs="C++20|C++23" %}
```cpp
// C++20: a protected virtual method, overridden by a test subclass.
class ShipmentEtaScheduler {
public:
    ScheduledEta schedule_shipment(const std::string& shipment_id) const {
        const auto dispatched_at = current_time();
        const auto promised_by = dispatched_at + std::chrono::days(transit_days_);
        return ScheduledEta{shipment_id, dispatched_at, promised_by};
    }

protected:
    virtual std::chrono::system_clock::time_point current_time() const;

private:
    int transit_days_;
};

// A test-only subclass overrides just the one call.
class FixedClockEtaScheduler : public ShipmentEtaScheduler {
protected:
    std::chrono::system_clock::time_point current_time() const override { return fixed_now_; }
};
```
```cpp
// C++23: an explicit object parameter instead of a virtual function.
// schedule_shipment() becomes a function template taking `this Self&&
// self`; calling self.current_time() resolves against Self's static type
// at compile time, not at runtime through a vtable.
class DeducingEtaScheduler {
public:
    template <typename Self>
    ScheduledEta schedule_shipment(this Self&& self, const std::string& shipment_id) {
        const auto dispatched_at = self.current_time();
        const auto promised_by = dispatched_at + std::chrono::days(self.transit_days_);
        return ScheduledEta{shipment_id, dispatched_at, promised_by};
    }

    std::chrono::system_clock::time_point current_time() const;
};

// No `virtual`, no `override`: this derived class's current_time() hides
// the base one by ordinary member-name lookup once Self deduces to it.
class FixedClockDeducingEtaScheduler : public DeducingEtaScheduler {
public:
    std::chrono::system_clock::time_point current_time() const { return fixed_now_; }
};
```

Either way, a test builds a scheduler whose clock returns a known timestamp and asserts the exact `dispatched_at` and `promised_by`. The forms differ in cost:

| | C++20 virtual | C++23 deducing this |
| --- | --- | --- |
| Dispatch | Through the vtable, at run time | Resolved when the template instantiates |
| Per-object cost | A vtable pointer | None |
| Per-call cost | An indirect call to `current_time()` | A direct call |
| Override syntax | `virtual` and `override` | A same-named member that hides the base one |

Deducing this keeps the seam without paying for it in production.

## Encapsulate Global References: wrapping the read, not removing the global

A fuel-surcharge multiplier lives in a mutable global the pricing module reads directly, set at startup by an operations console this codebase doesn't own. Removing it means coordinating with the console's owner. Encapsulate Global References wraps the read instead:

```cpp
class FuelSurchargeSource {
public:
    virtual ~FuelSurchargeSource() = default;
    virtual double multiplier() const = 0;
};

class LiveFuelSurchargeSource : public FuelSurchargeSource {
public:
    double multiplier() const override;  // delegates to legacy_global::current_fuel_surcharge()
};

class ShipmentCostCalculator {
public:
    explicit ShipmentCostCalculator(const FuelSurchargeSource& source) : source_(source) {}
    double total_cost(double base_cost) const { return base_cost * source_.multiplier(); }
};
```

`ShipmentCostCalculator` never names the global. Production wires it to `LiveFuelSurchargeSource`, which reads the global from one place instead of everywhere. A calculator built on a fixed source keeps returning the same total while the legacy global is set to other values. The global still exists; this calculator no longer cares.

## Introduce Instance Delegator: a seam in front of a static API nobody can touch

`LegacyCarrierZoneLookup` is a static-only utility, `static int zone_for(const std::string&)`, called directly from many places; `ZoneSurchargeReport` stands in for one of those callers. Changing the static method or replacing it with an instance design means touching every caller in the same change.

Introduce Instance Delegator leaves `LegacyCarrierZoneLookup` and `ZoneSurchargeReport` alone. A thin instance class implements a new interface and forwards to the static method:

```cpp
class ZoneLookupPort {
public:
    virtual ~ZoneLookupPort() = default;
    virtual int zone_for(const std::string& postal_code) const = 0;
};

class CarrierZoneLookupDelegator : public ZoneLookupPort {
public:
    int zone_for(const std::string& postal_code) const override {
        return LegacyCarrierZoneLookup::zone_for(postal_code);
    }
};
```

New code, `ShipmentRoutingAdvisor`, depends on `ZoneLookupPort` from the start, so a test drives it through a GoogleMock `MockZoneLookupPort` with no legacy table involved. The delegator forwards to the static method unchanged, and `ZoneSurchargeReport`'s existing call keeps working. It adds no logic; it's an object a test can replace, which a static method can't be.

{% include excalidraw.html name="28-instance-delegator" caption="Figure 28.2: ShipmentRoutingAdvisor depends on ZoneLookupPort, a seam a delegator forwards to the untouched static lookup, while ZoneSurchargeReport's existing call site keeps calling that static method directly." %}

> **🤖 Explore with your agent**
>
> "In `examples/28-dependency-breaking`, apply Introduce Instance Delegator. Leave `LegacyCarrierZoneLookup` and `ZoneSurchargeReport` byte-for-byte unchanged. Add a `ZoneLookupPort` interface with `virtual int zone_for(const std::string& postal_code) const = 0`, and a `CarrierZoneLookupDelegator` that forwards to `LegacyCarrierZoneLookup::zone_for`. Make `ShipmentRoutingAdvisor` take a `const ZoneLookupPort&` and never name the static class. Add a GoogleMock `MockZoneLookupPort` and drive the advisor through it with no legacy table."
>
> Check with `ctest --preset cpp23 -R ShipmentRoutingAdvisor`, and confirm `git diff` shows no change to `LegacyCarrierZoneLookup` or `ZoneSurchargeReport`.

## Adapt Parameter: shrinking a dependency to what it actually reads

`WarehouseDispatcher::dispatch()` took a `CarrierManifestFile`, a legacy type whose constructor opens and parses a real file. Every test of `dispatch()` needed a file on disk, though the method reads only `carrier_code()` and `shipment_count()`.

Adapt Parameter puts that surface in its own interface and wraps the legacy type to satisfy it:

```cpp
class ManifestSource {
public:
    virtual ~ManifestSource() = default;
    virtual int shipment_count() const = 0;
    virtual std::string carrier_code() const = 0;
};

class CarrierManifestFileAdapter : public ManifestSource {
public:
    explicit CarrierManifestFileAdapter(const CarrierManifestFile& file) : file_(file) {}
    int shipment_count() const override { return file_.shipment_count(); }
    std::string carrier_code() const override { return file_.carrier_code(); }
};
```

`dispatch()` now takes `const ManifestSource&`, and `CarrierManifestFile` is unchanged. The large-batch and small-batch rules run against a two-field fake with no filesystem access. A real `CarrierManifestFile` built from a temporary file and wrapped in the adapter produces the same `DispatchSummary` as the fake, so the adapter is a faithful stand-in. Adapt Parameter is Extract Interface applied to a parameter type the method can't redesign.

## Build, run, observe

From the repository root, the superbuild picks up `examples/28-dependency-breaking`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 14 tests pass under `cpp23` and all 13 under `cpp20`; the difference is the deducing-this case for `DeducingEtaScheduler`, which only compiles under C++23. To run the techniques against production wiring:

```sh
examples/28-dependency-breaking/demo.sh cpp23
```

```
== Extract Interface + Parameterize Constructor: ShipmentAlertNotifier ==
  [smtp:mail.internal.example] to=ops-supervisor@example.com "Shipment SH-5001 flagged for manual review: weight exceeds declared value"
  recorded (no real SMTP call): to=ops-supervisor@example.com "Shipment SH-5002 flagged for manual review: recipient address flagged"

== Extract and Override Call: ShipmentEtaScheduler ==
  live schedule for SH-6001 computed against the real clock
  deducing-this variant (C++23) computed the same shape, no vtable involved

== Encapsulate Global References: ShipmentCostCalculator ==
  live total for a $100.00 base at the legacy global's current rate: $118.00

== Introduce Instance Delegator: ShipmentRoutingAdvisor ==
  local hub, zone 1
  legacy call site unchanged: zone 3 surcharge report for 99501

== Adapt Parameter: WarehouseDispatcher ==
  dispatched 275 shipments for FASTFREIGHT (large batch)
```

The second line of the first block is the notifier built with a `RecordingAlertChannel` instead of `SmtpAlertSender`: the same `flag_for_manual_review()` logic formats the same message, and nothing in `ShipmentAlertNotifier` knows the channel changed. The `cpp20` demo prints the same output minus the deducing-this line.

## When not to use it

Apply a technique when a test needs the seam it opens. Before that, it's speculative generality: an `AlertChannel` extracted from a mail dependency no test will ever substitute is indirection bought for nothing. Name what you want to test, find the dependency blocking it, and use the one technique that breaks it.

The techniques get code under test; they don't improve its design, and Feathers says so. `ShipmentRoutingAdvisor` on `ZoneLookupPort` is more testable, not better organized, and the zone logic in `LegacyCarrierZoneLookup` is as opaque as before. Once characterization tests exist and a dependency is broken, Fowler's named refactorings (Extract Function, Rename Variable, Replace Conditional with Polymorphism) improve the design, with the tests holding it in place.

Prefer the cheapest technique that fits:

| Where the dependency enters | Reach for |
| --- | --- |
| Through a parameter list a caller controls | Parameterize Constructor or Parameterize Method |
| Inside a method body, with no parameter to extend without changing every caller | Extract and Override Call or Subclass and Override Method |
| Only reachable by swapping the symbol at build time, every structural technique ruled out | Text Redefinition, the preprocessor seam, accepting the one-definition-rule risk |

## For deeper coverage

- Michael Feathers, [*Working Effectively with Legacy Code*](https://www.oreilly.com/library/view/working-effectively-with/0131177052/) (Prentice Hall): the source of all 24 techniques, with worked examples in languages beyond C++ and the seam vocabulary behind this part.
- Martin Fowler, [*Refactoring: Improving the Design of Existing Code*](https://martinfowler.com/books/refactoring.html), 2nd ed. (Addison-Wesley): the named-refactoring catalogue to apply once a dependency is broken and a characterization test exists.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/28-dependency-breaking`, including GoogleMock doubles that verify an interaction rather than only reading state back.
- The full reading list is on the [bibliography page]({{ '/bibliography/' | relative_url }}).

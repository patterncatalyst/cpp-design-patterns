---
title: "The 24 dependency-breaking techniques for C++20/23"
order: 28
part: "changing-inherited-code"
description: "Michael Feathers' 24 dependency-breaking techniques, catalogued in full and modernized for C++20/23, then demonstrated on inherited shipping code: a mail dependency pulled behind an interface and opened for injection, a wall-clock read isolated into an overridable call (virtual in C++20, a deducing-this hook in C++23), a mutable global wrapped behind an accessor, a static-only legacy lookup given an instance delegator, and a file-parsing parameter adapted down to the two values a caller actually reads."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Name all 24 of Feathers' dependency-breaking techniques and the modern C++20/23 tool that expresses each one today.
- Apply Extract Interface and Parameterize Constructor together to open a seam in a class that currently builds its own collaborator, without touching a single existing call site.
- Apply Extract and Override Call to isolate a single troublesome expression, such as a wall-clock read, into its own overridable method, and express the same seam with a C++20 virtual method or a C++23 deducing-this hook.
- Apply Encapsulate Global References to wrap a mutable global behind an interface, so new code can depend on the interface while the global itself stays exactly where it was.
- Apply Introduce Instance Delegator to give new code an injectable collaborator in front of a static-only legacy API, with no change to the legacy type or any of its existing callers.
- Apply Adapt Parameter to shrink a method's dependency on a heavyweight concrete parameter down to the handful of values it actually reads, so a test never has to construct the heavyweight type.
- Decide, for a given piece of inherited code, which dependency-breaking technique costs the least for the seam it buys, rather than reaching for the first one that comes to mind.

{% include excalidraw.html name="28-dependency-breaking" caption="Figure 28.1: the same call site, two shapes. Before, ShipmentAlertNotifier constructs and calls SmtpAlertSender directly, with no seam. After Extract Interface and Parameterize Constructor, the notifier depends on an AlertChannel interface, and either a production SmtpAlertSender or a test double can sit behind it." %}

## Why this matters

Inherited code earns that name because changing it carries risk nobody fully understands anymore, and the risk rarely comes from the logic itself. A pricing calculation, a routing decision, a notification rule, these are usually a few lines any reader can follow in isolation. The risk comes from what the logic is wired to: a concrete mail client constructed with `new` in a constructor body, a direct read of `std::chrono::system_clock::now()` buried in the middle of a method, a global variable an operations console sets at startup, a static utility class called from forty places, a parameter type that opens a file the moment it is constructed. None of these wirings is a bug. Every one of them was a reasonable choice when it was written, often the most direct way to say what the code needed to do. The cost shows up later, the day someone needs to write a test and discovers that calling one method on a thirty-line class means standing up a real SMTP connection, or waiting on the real clock, or reading and writing a real file.

Feathers' *Working Effectively with Legacy Code* names this problem precisely and then does something unusual for a methodology book: it refuses to offer one universal fix. Instead it catalogues 24 distinct techniques, each one a small, mechanical move that breaks exactly one kind of dependency, chosen because together they cover nearly every shape a hard-wired collaborator can take in an object-oriented codebase. A seam, in Feathers' vocabulary, is a place where behavior can change without editing the code in that place, and every technique in the catalogue exists to open one specific kind of seam. Extract Interface opens a seam around a concrete type's public surface. Parameterize Constructor opens a seam at the point of construction. Encapsulate Global References opens a seam around a value nobody owns. The techniques differ in mechanism, but every one of them answers the same question: given this particular hard-wired dependency, what is the smallest change that lets a test substitute something else in its place?

That smallest-change discipline is the entire argument for working through a reference catalogue instead of inventing a fix from scratch each time. A team facing a global variable and a team facing a static-only utility class are facing structurally different problems, and the fix that works for one does nothing for the other. Reaching for "add dependency injection" as a blanket answer to both usually means a much larger rewrite than either problem actually requires, touching code that did not need to change and introducing risk the original problem never carried. The 24 techniques stay narrow by design. This chapter's worked subset, Extract Interface, Parameterize Constructor, Extract and Override Call, Encapsulate Global References, Introduce Instance Delegator, and Adapt Parameter, demonstrates that smallness directly: every one of the five pieces of inherited shipping code this chapter touches keeps its existing behavior and every one of its existing callers, while gaining exactly the seam a test needs and nothing more.

## The full catalogue, mapped to modern C++20/23

Feathers wrote the catalogue against the C++ of the mid-2000s, when `virtual` and a hand-written interface were close to the only tools available for opening a seam. C++20 and C++23 add real alternatives for several entries: concepts for structural rather than inheritance-based interfaces, `std::function` and `std::move_only_function` for a pluggable callable in place of a hand-rolled strategy object, and deducing this for static polymorphism that reads like a virtual override without paying for one. The table below carries the full catalogue; the six techniques worked through in the sections that follow are marked so the table doubles as an index into the runnable example.

| Technique | What it breaks | Modern C++20/23 expression |
|---|---|---|
| Adapt Parameter★ | A method depends on a heavyweight concrete parameter type it only reads a few values from | Extract a minimal abstract interface (or a concept-constrained template parameter) for just the needed surface; wrap the heavyweight type in an adapter |
| Break Out Method Object | A long method's locals and logic resist isolation inside the method itself | Turn the method body into a small class or a call-once local struct with `operator()`, capturing what used to be locals as members |
| Definition Completion | A declared symbol has no definition a test build can link against | Provide a test-only translation unit supplying the missing definition, or an explicit template instantiation for a template declaration |
| Encapsulate Global References★ | Code reads or writes a global variable inline, wherever it needs the value | Wrap the global behind an interface; inject the wrapper so new code never names the global directly |
| Expose Static Method | Logic that needs no instance state is trapped inside an instance method, forcing a full object just to call it | Move the logic to a `static` member function or a free function in a detail namespace |
| Extract and Override Call★ | One troublesome call sits inline inside an otherwise-fine method | Pull the call into its own method; override it virtually (C++20) or hide it via a deducing-this hook (C++23) |
| Extract and Override Factory Method | An embedded `new` expression hard-wires which concrete type gets constructed | Move the construction into its own virtual (or deducing-this) factory method a test subclass overrides |
| Extract and Override Getter | A troublesome expression, such as a global read or an expensive computation, is inlined at every use | Move the expression behind a virtual (or deducing-this) getter a test subclass overrides |
| Extract Implementer | A class mixes a contract worth keeping with an implementation that needs to vary | Split into an abstract interface and a concrete implementer, or a `std::function`-based type eraser for a narrow contract |
| Extract Interface★ | A dependent class can only ever be built against one concrete collaborator type | Pull the collaborator's minimal contract into an abstract base class, so a double can implement just that contract |
| Introduce Instance Delegator★ | A static-only or free-function API is called from many places that cannot all change at once | Wrap the static API in a thin instance class implementing a new interface, forwarding every call through |
| Introduce Static Setter | A global needs to be substitutable by a test but has no write path | Add a static setter (paired with an RAII guard that resets the value afterward) |
| Link Substitution | A dependency needs to be replaced at the object-file level, with no source change | Build a test-only translation unit providing the same symbols, linked only into the test target |
| Parameterize Constructor★ | A constructor builds its own collaborator internally, with no way for a caller to supply another one | Add a constructor parameter for the collaborator, defaulted to the original construction so every existing call site still compiles |
| Parameterize Method | A single method, not the whole object, reaches for a hard-wired collaborator | Pass the collaborator as a method argument instead of reaching for it internally |
| Primitivize Parameter | A method's real dependency is on a couple of primitive values buried inside a hard-to-construct object parameter | Change the signature to accept the primitive values directly |
| Pull Up Feature | A method or field needed by a test lives on a subclass that pulls in dependencies the test does not want | Move the feature to a shared base class, away from the dependency-heavy subclass |
| Push Down Dependency | A base class carries a dependency only some of its subclasses actually need | Move the dependency down into just the subclasses that use it, freeing the base class (and its tests) from it |
| Replace Function with Function Pointer | A hard C-style function call cannot be intercepted by any object-oriented seam | Replace the call with a function-pointer or `std::function`/`std::move_only_function` member a test can repoint |
| Replace Global Reference with Getter | A direct global read sits inline inside a method body, not at the point of construction | Replace the read with a call to a virtual (or injected) getter |
| Subclass and Override Method★ | Any method with a troublesome implementation that a test needs to suppress or replace | Create a test-only subclass overriding just that method; the move underlying several of the entries above |
| Supersede Instance Variable | A collaborator is wired into an already-built object with no constructor seam available yet | Add a setter that replaces the collaborator on a live instance, as an interim step toward a real constructor seam |
| Template Redefinition | A class needs a different collaborator type for tests, resolved entirely at compile time | Parameterize the class on the collaborator type as a template parameter, optionally constrained with a concept |
| Text Redefinition | In dynamic or interpreted languages, the source text itself is patched for a test build | In C++, the closest and most discouraged analog: a preprocessor seam swapping a symbol for a test build, used only as a last resort |

\* Demonstrated in `examples/28-dependency-breaking`.

Several entries deserve a word beyond the table. Template Redefinition is the technique C++ is unusually well suited for among the languages Feathers wrote against: a class template parameterized on its collaborator type gets a compile-time seam with no virtual call and no interface to maintain, at the cost of every client needing to know the template parameter. Text Redefinition sits at the opposite end of the table: Feathers describes it as a last resort even in the languages where it is easy, and a `#define`-based seam in C++ carries real risk of violating the one-definition rule across translation units. Reach for it only when every technique above it on this table has already been ruled out for a specific dependency.

## Extract Interface and Parameterize Constructor: opening a mouth for a seam

The inherited `ShipmentAlertNotifier` sends a message to a warehouse supervisor whenever a shipment gets flagged for manual review. Before this chapter's pass, its constructor took no arguments and built its own mail dependency internally, a concrete `SmtpAlertSender` with no interface anywhere near it. That shape is common in inherited code for a reason that has nothing to do with bad judgment: the author needed exactly one way to send a message, writing an interface for a single implementation would have looked like speculative generality, and nobody involved was thinking about a test yet. The cost shows up only once someone tries to write one.

Extract Interface is the first move, and it targets the dependency, not the dependent. `AlertChannel` pulls the one method `ShipmentAlertNotifier` actually calls, `send(recipient, message)`, out of `SmtpAlertSender`'s full surface, which in a real system would also expose connection pooling and retry configuration nothing here touches:

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

Extracting the interface alone changes nothing about how `ShipmentAlertNotifier` gets its dependency; it only creates the possibility of a second implementation. Parameterize Constructor is the second move, the one that actually opens the seam, and the detail that makes it safe for inherited code is the default argument:

```cpp
explicit ShipmentAlertNotifier(
    std::string supervisor_email,
    std::shared_ptr<AlertChannel> channel = std::make_shared<SmtpAlertSender>("mail.internal.example"));
```

Every call site written against the old no-argument-dependency constructor, `ShipmentAlertNotifier notifier("ops@example.com");`, still compiles, and still behaves identically, because the default argument constructs the same real `SmtpAlertSender` the inherited constructor used to build internally. A test, and only a test, supplies the second argument: `tests/shipment_alert_notifier_test.cpp` passes a GoogleMock `MockAlertChannel` wrapped in a non-owning `shared_ptr` and sets an `EXPECT_CALL` on it, verifying the exact recipient and a substring of the message, then separately exercises a plain recording fake to confirm the formatted text. Neither test opens a socket. The pairing of these two techniques names two different questions: Extract Interface answers "what could this dependency be," and Parameterize Constructor answers "how does a caller supply a different one." This chapter's example needs both halves for the seam to exist at all.

## Extract and Override Call: isolating the one call that moves

`ShipmentEtaScheduler::schedule_shipment()` validates a shipment and computes a promised delivery date from a transit-day count, logic this chapter leaves untouched because nothing about it is the problem. The problem is one line buried in the middle of the method, a direct call to `std::chrono::system_clock::now()` to stamp when the shipment was dispatched. That single call is the entire reason the method could never be tested with an exact assertion: every run produces a different "now," so a test could only ever check that the result falls in some range around the moment it ran, never confirm it computed one specific, known answer.

Extract and Override Call targets exactly this shape: one troublesome expression inlined inside an otherwise-fine method, too small to justify restructuring the whole method around it. The fix pulls the one call into its own method, changing nothing about when it runs or what it returns in production:

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

Both forms give a test the identical capability: construct a scheduler whose clock returns a known timestamp, call `schedule_shipment()`, and assert the exact `dispatched_at` and `promised_by` values, which `tests/shipment_eta_scheduler_test.cpp` does under both presets. The difference between them is real, not stylistic. The C++20 form needs a vtable pointer in every `ShipmentEtaScheduler` instance and pays an indirect call on every `current_time()` invocation, a cost accepted for a method that varies only in a test build and never in production. The C++23 form resolves `self.current_time()` against `Self`'s type when the template instantiates, at compile time, so `FixedClockDeducingEtaScheduler`'s own `current_time()` is selected with no indirection at all. The substitution a test needs, subclass and override one method, is available either way; deducing this removes the runtime cost of carrying that seam into production code that will never actually exercise the overridden path.

## Encapsulate Global References: wrapping the read, not removing the global

A fuel-surcharge multiplier lives in a mutable global the inherited pricing module reads directly, set once at startup by an operations console nothing in this codebase owns. Encapsulate Global References does not propose removing that global, which would mean coordinating a change with whatever system owns the startup console, almost certainly out of scope for whoever is trying to get a single pricing method under test today. Instead it wraps the read behind an interface:

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

`ShipmentCostCalculator` never names the global; it depends on `FuelSurchargeSource`, and production code wires it to `LiveFuelSurchargeSource`, which does the one thing the inherited code used to do everywhere, read the global, now concentrated behind one method. `tests/shipment_cost_calculator_test.cpp` constructs a calculator against a `FixedFuelSurchargeSource` holding a known rate, confirms the total, then sets the legacy global to two more different values and confirms the total never moves. The test is not proving the global is gone; it is proving a specific calculator instance never has to care that the global exists at all, which is the value Encapsulate Global References buys for code that cannot remove the global it depends on.

## Introduce Instance Delegator: a seam in front of a static API nobody can touch

`LegacyCarrierZoneLookup` is a static-only utility, `static int zone_for(const std::string&)`, called directly from many places elsewhere in the inherited codebase; `ZoneSurchargeReport` in this chapter's example stands in for one of those call sites. Changing the static method's signature, or replacing it with an instance-based design outright, would mean touching every one of those callers in the same change, a scope no single test-writing task should have to accept.

Introduce Instance Delegator solves this without touching `LegacyCarrierZoneLookup` or `ZoneSurchargeReport` at all. A thin instance class implements a new interface and forwards every call straight to the static method:

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

New code, `ShipmentRoutingAdvisor` in this chapter's example, is written against `ZoneLookupPort` from the start, never against the static method directly. `tests/shipment_routing_advisor_test.cpp` confirms the delegator forwards correctly, drives `ShipmentRoutingAdvisor` through a GoogleMock `MockZoneLookupPort` with no legacy table involved at all, and separately confirms `ZoneSurchargeReport`'s existing call site still compiles and runs exactly as it did before the delegator existed. The delegator adds nothing to the lookup logic; its entire job is being an object a caller can hold a reference to and a test can replace, which a bare static method can never be.

{% include excalidraw.html name="28-instance-delegator" caption="Figure 28.2: ShipmentRoutingAdvisor depends on ZoneLookupPort, a seam a delegator forwards to the untouched static lookup, while ZoneSurchargeReport's existing call site keeps calling that static method directly." %}

> **AI sidebar.** Ask an LLM pairing partner to "make this legacy class testable" and a common answer is a full rewrite: convert every static method to an instance method, inject every dependency through a new constructor, and update every call site in the same change. That answer is not wrong about where a codebase should eventually land, and it is often the worse first move, because it bundles an untested, high-risk rewrite with the very thing you were trying to achieve, a passing test. Feathers' catalogue exists because the smaller move, Introduce Instance Delegator here, Parameterize Constructor with a default argument elsewhere, gets the same seam with a change small enough to review in minutes and with zero existing callers touched. When a generated suggestion reaches for the comprehensive rewrite, ask it the question this chapter's catalogue is built around: what is the smallest technique that opens the specific seam this test needs, and does it require touching any call site that doesn't already know it is being tested?

## Adapt Parameter: shrinking a dependency to what it actually reads

`WarehouseDispatcher::dispatch()` used to take a `CarrierManifestFile` parameter directly, a legacy type whose constructor opens a real file on disk and parses it. Every test that wanted to call `dispatch()` needed a real file, a cost disproportionate to what the method actually does with the parameter: it reads `carrier_code()` and `shipment_count()` and nothing else.

Adapt Parameter extracts exactly that minimal surface into its own interface, then wraps the heavyweight legacy type to satisfy it:

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

`WarehouseDispatcher::dispatch()` now takes `const ManifestSource&` instead of `const CarrierManifestFile&`, and `CarrierManifestFile` itself is unchanged; nothing about its file-parsing constructor needed to move or disappear. `tests/warehouse_dispatcher_test.cpp` drives the large-batch and small-batch logic entirely through a plain `FakeManifestSource` holding two fields, with no filesystem access anywhere in either test. A third test builds a real `CarrierManifestFile` from a temporary file and wraps it in the adapter, confirming the adapted path produces the identical `DispatchSummary` the fake path does, proof that the adapter is a faithful stand-in and not just a convenient shortcut that happens to pass. Adapt Parameter is the mirror image of Extract Interface applied at a call boundary instead of at a class's own public surface: Extract Interface shrinks what a dependent needs from a collaborator it owns the relationship to, while Adapt Parameter shrinks what a method needs from a parameter it does not get to redesign.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain from the prerequisites; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/28-dependency-breaking` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 14 cases under the `cpp23` preset, 13 under `cpp20`, the difference being the one deducing-this test for `DeducingEtaScheduler` that only compiles under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 14   # cpp23
100% tests passed, 0 tests failed out of 13   # cpp20
```

To see all six techniques run against production wiring at the terminal, run the example's own demo script:

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

The second line of the first block is the chapter's argument in one run: `test_style_notifier` is constructed with a `RecordingAlertChannel` instead of a real `SmtpAlertSender`, and `flag_for_manual_review()` runs its identical logic, formatting the identical message, with nothing in `ShipmentAlertNotifier` aware that anything changed. Running `examples/28-dependency-breaking/demo.sh cpp20` prints the same output minus the deducing-this line, which only compiles under the `cpp23` preset.

## When not to use it

A dependency-breaking technique is worth applying the moment a real test needs the seam it opens. Applying one before that moment is the same speculative-generality mistake this book warns against everywhere else: an `AlertChannel` interface extracted from a mail dependency that no test, present or planned, will ever substitute is a layer of indirection bought for nothing. The discipline these techniques share with every pattern in this book's catalogue is forces-first: name the specific test you are trying to write, find the specific dependency blocking it, then reach for the one technique in the table that breaks exactly that dependency, no more.

The techniques also have a real ceiling, and Feathers is explicit about where it sits: they get code under test, they do not improve its design. `ShipmentRoutingAdvisor` depending on `ZoneLookupPort` instead of a static method is more testable, not better organized; the zone-boundary logic inside `LegacyCarrierZoneLookup` is exactly as opaque after Introduce Instance Delegator as before it. Once characterization tests exist and a dependency is broken, the named refactorings from Fowler's catalogue, extract method, rename, replace conditional with polymorphism, are the tools for improving the design itself; this chapter's techniques exist only to make that later work safe, by giving a test something to hold onto while it happens.

Order matters inside the catalogue, too. Reach for Parameterize Constructor or Parameterize Method first whenever a dependency enters through a parameter list a caller already controls, since nothing beats a plain argument for simplicity and compile-time safety. Reach for Extract and Override Call or Subclass and Override Method only when the dependency is embedded inside a method body itself, with no parameter list to extend without changing the method's contract for every caller. And reserve Text Redefinition, the preprocessor seam, for the rare case where every structural technique above it has been tried and ruled out; a `#define`-based substitution risks violating the one-definition rule the moment two translation units see different values for the same symbol, a cost worth paying only when nothing safer is available.

## For deeper coverage

- Michael Feathers, [*Working Effectively with Legacy Code*](https://www.oreilly.com/library/view/working-effectively-with/0131177052/) (Prentice Hall): the source of all 24 techniques this chapter modernizes, with the full worked examples in languages beyond C++ and the seam vocabulary this book's inherited-code part is built on.
- Martin Fowler, [*Refactoring: Improving the Design of Existing Code*](https://martinfowler.com/books/refactoring.html), 2nd ed. (Addison-Wesley): the named-refactoring catalogue to apply once a dependency is broken and a characterization test exists, picking up exactly where this chapter's techniques stop.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/28-dependency-breaking`'s suite, including the GoogleMock collaborator doubles that verify an interaction rather than only reading state back.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.

---
title: "Anti-patterns and pattern misuse"
order: 26
part: "patterns-in-the-wild"
description: "Recognizing patternitis in a shipping codebase: a singleton configuration registry holding global mutable state, a factory-of-factories wrapped around interfaces with one implementation each, a god object that validates, logs, and formats, and a leaky abstraction that exposes its own call order. The simplified rewrite collapses five types into one class and one value, and the test suite proves both that it computes the same answer and that it is testable where the original was not."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Recognize a singleton used as global mutable state, name the specific cost it imposes beyond the Singleton pattern's usual tradeoffs, and show it with a test that forces the cost into the open.
- Recognize a god object by what it knows rather than by its line count, and separate "too many responsibilities" from "too much code."
- Recognize speculative generality: a Strategy, a Factory, or any interface carrying exactly one implementation with no second one in sight.
- Recognize a leaky abstraction where a type's public surface forces a caller to understand internal, call-order-dependent state instead of hiding it.
- Apply the forces vocabulary as a diagnostic: a pattern applied where no force calls for it is the anti-pattern, independent of whether the code compiles or even works.
- Rewrite an over-patterned design down to the smallest shape its actual requirements justify, and prove with a test that the rewrite is both behaviorally identical and more testable than what it replaced.

## Why this matters

Every pattern in this book's catalogue earns its chapter because a real force justifies it: Strategy when behavior varies by a condition a caller controls, Factory when which concrete type to build is itself a decision, Singleton when exactly one instance must exist and nothing needs to substitute a different one. The forces vocabulary matters here for a specific reason: the same catalogue that teaches a pattern's benefits also teaches its shape, and a shape is something an engineer can apply from memory long after the forces that justified it have stopped applying, or without ever having checked whether they applied in the first place.

The result has a name worth knowing: patternitis, the reflex of reaching for a named pattern because its shape is familiar, not because anything in the code in front of you calls for it. Patternitis rarely shows up as one visibly wrong line. It shows up as a design that compiles, passes code review because every piece looks textbook, and still costs far more to read, test, and change than the problem underneath it would have cost to solve directly. This chapter takes a shipment-pricing calculation, multiply a base cost by a fuel surcharge and add a handling fee, and builds the over-patterned version a codebase accumulates one reasonable-sounding decision at a time: a configuration registry becomes a singleton because singletons are convenient to reach from anywhere, the object it hands out gets resolved through a factory because factories decouple construction from use, the factory itself gets resolved through a second singleton because why wire it by hand when a global accessor is right there, and the class that ties it together picks up logging and formatting along the way because nothing else owned those jobs yet. Every individual step is defensible in isolation. The design that results needs five types and two singletons to multiply one number by another.

{% include excalidraw.html name="26-anti-patterns" caption="Figure 26.1: the over-patterned design chains a god object through a factory-of-factories, an interface with one implementation, and a singleton holding global mutable state; the simplified design injects one plain value into one class and gets one value back." %}

## Singleton as global mutable state

A Meyers singleton, even when built correctly, imposes four costs: global mutable state, a hidden dependency, a hard-to-substitute implementation, and static-initialization-order risk. This chapter's `ShippingRateRegistry` (`examples/26-anti-patterns/src/shipping_rate_registry.hpp`) is built exactly as correctly, with the same thread-safe function-local static and the same deleted copy operations. The mechanism is not where this chapter's complaint lives. The complaint is what the mechanism is used for: a single mutable field, the fuel surcharge multiplier, that more than one part of a pricing pipeline believes it can configure independently.

```cpp
class ShippingRateRegistry {
public:
    static ShippingRateRegistry& instance();
    void set_surcharge_multiplier(double multiplier);
    double surcharge_multiplier() const;
    // ...
private:
    mutable std::mutex mutex_;
    double surcharge_multiplier_ = 1.0;
};
```

`StandardRateCalculator::calculate()` reads `ShippingRateRegistry::instance().surcharge_multiplier()` every time it runs, rather than capturing a rate once at construction. That single design choice is what turns a convenient global accessor into a correctness bug waiting on a test to expose it. `tests/shipping_rate_registry_singleton_test.cpp` writes that test directly: a "region A" calculator is obtained while the registry holds one rate, and its answer is confirmed. A "region B" calculator is then obtained after a second, unrelated call sets the registry to a different rate. Region A's calculator, untouched by any of that, reports region B's rate the next time anyone asks it:

```cpp
ShippingRateRegistry::instance().set_surcharge_multiplier(1.10);
auto region_a = factory.create_calculator();
EXPECT_DOUBLE_EQ(region_a->calculate(100.0), 110.0);

ShippingRateRegistry::instance().set_surcharge_multiplier(1.30);
auto region_b = factory.create_calculator();
EXPECT_DOUBLE_EQ(region_b->calculate(100.0), 130.0);

// region_a was never touched directly, yet it now reports region B's rate.
EXPECT_DOUBLE_EQ(region_a->calculate(100.0), 130.0);
```

That is spooky action at a distance, a term physicists use for an effect with no visible causal link to its cause, borrowed into software for exactly this situation: two objects that look independent, share one mutable slot, and change each other's behavior through no code path a reader can see by looking at either one. The structural consequence is the one this chapter's AFTER design exists to fix: a singleton configuration slot cannot hold two independently valid configurations at once, because there is only one slot. A program that needs to price shipments for two regions at the same time, with two different surcharge rates live simultaneously, cannot express that requirement through `ShippingRateRegistry` at all. It would need a second singleton, which only moves the same one-slot problem to a different name.

## Speculative generality: an interface with no second implementation

`RateCalculator` and `RateCalculatorFactory` (`examples/26-anti-patterns/src/rate_calculator.hpp` and `rate_calculator_factory.hpp`) are both abstractions with exactly one concrete type behind them, `StandardRateCalculator` and `StandardRateCalculatorFactory`, and neither has a second implementation anywhere in this codebase or a credible one on the horizon. That is speculative generality: an abstraction built for a variation that has not arrived, justified by the possibility that it might. Factory Method earns its place because choosing which concrete type to build is sometimes itself a real decision, resolved by a config value, a runtime flag, or a registered family of related types. `StandardRateCalculatorFactory::create_calculator()` does exactly one thing:

```cpp
std::unique_ptr<RateCalculator> StandardRateCalculatorFactory::create_calculator() const {
    return std::make_unique<StandardRateCalculator>();
}
```

Nothing about this call ever varies. A factory wrapped around a constructor that only ever builds one type is a layer of indirection with nothing underneath it: a reader has to open `RateCalculatorFactory`, confirm it has one implementation, then open that implementation to find the actual logic, three hops to reach code that `std::make_unique<StandardRateCalculator>()` would have put on the same line as the call site.

`RateCalculatorFactoryProvider` compounds the problem rather than fixing it. It is a second singleton, built for the sole purpose of handing out the one `RateCalculatorFactory` this program will ever construct:

```cpp
RateCalculatorFactory& RateCalculatorFactoryProvider::factory() {
    return *factory_;
}
```

A caller three function calls away from `main()` reaches a concrete `StandardRateCalculator` through `RateCalculatorFactoryProvider::instance().factory().create_calculator()`, four symbols and two singletons deep, to get the exact same object a direct `std::make_unique<StandardRateCalculator>()` would hand back in one line. This is the shape the book's title calls a factory of factories: speculative generality compounding with Singleton's hidden-dependency cost, each one hiding the other's problem well enough that a reviewer skimming any single class in isolation can miss both.

> **AI sidebar.** Ask an LLM pairing partner to "make this configurable" or "add a factory in case we need a different implementation later" and it will usually comply exactly as asked, because the shape is common in training data and nothing about producing it is technically wrong. What a generated answer will not do on its own is push back and ask whether a second implementation is actually coming, or whether the interface it just wrote has any caller that will ever supply anything other than the one concrete type already in the file. Check the forces before accepting the suggestion: is there a config value, a feature flag, or a second concrete type already sitting in a ticket somewhere that this abstraction exists to serve? If the answer is "not yet, but maybe," the generated factory is exactly this chapter's speculative generality, with a conversational tone attached that makes it sound like a best practice instead of a cost accepted with no benefit purchased.

## The god object: a class that knows everything

`ShipmentOperationsManager` (`examples/26-anti-patterns/src/shipment_operations_manager.hpp`) resolves a rate calculator through both singletons above, validates that a base cost is positive, writes an audit log entry for every shipment it prices, and formats a receipt string for display. Line count is not the test for whether this is a god object; `ShipmentOperationsManager` is barely forty lines. The test is the one-sentence question a reviewer should be able to answer about any class: what does it do? For `ShipmentOperationsManager` the answer needs four clauses, pricing orchestration, input validation, audit logging, and presentation formatting, and none of those four responsibilities has anything to do with the other three. A bug in the receipt-formatting string and a bug in the surcharge calculation are unrelated failures that nonetheless live in the same class, get reviewed in the same diff, and get tested by whatever test happens to exercise whichever method changed.

`tests/shipment_operations_manager_test.cpp` has to know about `operation_log()` purely to confirm that pricing a shipment had a side effect unrelated to pricing:

```cpp
EXPECT_TRUE(manager.operation_log().empty());
manager.price_shipment("ORD-3003", 50.0);
EXPECT_EQ(manager.operation_log().size(), 1U);
```

That test is not wrong to write. It is evidence that a single class's public surface now spans two concerns a test for either one has to wade through to reach the other. The Single Responsibility Principle, that a class should have one reason to change, is the same diagnostic restated for this chapter's purposes: `ShipmentOperationsManager` has four reasons to change, a pricing rule, a validation rule, a logging format, and a receipt layout, and any one of those four can force a recompile and a re-review of all four.

## Leaky abstraction: exposing what should stay hidden

The sharpest bug in this chapter's over-patterned design lives in one method: `ShipmentOperationsManager::was_last_calculation_surcharge_applied()`. It answers a real question, did the most recent pricing calculation apply a surcharge, but it answers it by tracking mutable state on the manager rather than by attaching the fact to the specific `PricedShipmentReceipt` that calculation returned:

```cpp
last_surcharge_multiplier_ = ShippingRateRegistry::instance().surcharge_multiplier();
// ...
bool ShipmentOperationsManager::was_last_calculation_surcharge_applied() const {
    return last_surcharge_multiplier_ != 1.0;
}
```

A caller who prices `ORD-3001` with no surcharge, holds onto the returned receipt, and later checks `was_last_calculation_surcharge_applied()` is not asking about `ORD-3001` at all if any other shipment has been priced in between. The method answers for whichever shipment was priced most recently, a fact its name does not disclose and its signature cannot express. `tests/shipment_operations_manager_test.cpp` proves exactly this: pricing `ORD-3001` with the default rate, then pricing `ORD-3002` with a surcharge, leaves `was_last_calculation_surcharge_applied()` reporting `true`, the correct answer for `ORD-3002` and a wrong one for anyone still holding the `ORD-3001` receipt and asking the manager about it. A leaky abstraction, in this book's vocabulary, is any type whose public surface fails to hide an internal detail, here the detail that the surcharge flag is ordered state tied to the last call rather than a fact about a specific result, forcing every caller to independently understand and track an implementation detail the type should have encapsulated.

## The simplified rewrite: one value, one class

`ShipmentPricingService` (`examples/26-anti-patterns/src/shipment_pricing_service.hpp`) replaces `ShippingRateRegistry`, `RateCalculator`, `StandardRateCalculator`, `RateCalculatorFactory`, `StandardRateCalculatorFactory`, and `RateCalculatorFactoryProvider`, six types across three files, with one small value and one small class:

```cpp
struct RateSettings {
    double surcharge_multiplier = 1.0;
    double handling_fee = 0.0;
};

struct PricingResult {
    double total_cost = 0.0;
    bool surcharge_applied = false;
};

class ShipmentPricingService {
public:
    explicit ShipmentPricingService(RateSettings settings) : settings_(settings) {}
    PricingResult price(double base_cost) const;
private:
    RateSettings settings_;
};
```

`RateSettings` is the entire configuration surface the six-type design spent itself expressing: a surcharge multiplier and a handling fee, both known at construction time and never needing to change out from under a caller mid-calculation. Nothing here varies polymorphically, so nothing here needs an interface; a plain, copyable value is the correct level of abstraction for data with this shape, the same judgment that applies to value types generally. `PricingResult` is the direct fix for the leaky abstraction above: `surcharge_applied` travels with the `total_cost` it describes, on the same value, so there is no window where a caller can ask the wrong question about the wrong shipment. Whatever validation, logging, or formatting a real system still needs would each get its own small type under this design, not because this chapter is squeamish about four responsibilities existing somewhere, but because each one earns its own class exactly when something, a second log format, a second validation rule, a second receipt layout, gives it a reason to vary independently of the others, which is the same forces-based test that applies to every pattern in this book's catalogue.

## Proving equivalence, and proving the testability gap

A simplified design is only convincing if it computes the same answer as the design it replaces and if the claimed testability gap is something a test can show, not just assert. `tests/equivalence_test.cpp` drives both designs through the same table of base costs and surcharge rates and checks the totals agree exactly, plus a shared check that both reject a non-positive base cost with the same exception. That test is what makes every other claim in this chapter trustworthy: the two designs are not being compared on dissimilar problems, they compute the identical number.

The testability gap is `tests/shipment_pricing_service_test.cpp`'s subject, and it runs the exact scenario `ShippingRateRegistry`'s single mutable slot cannot support: two independently configured instances, alive and in use in the same process, neither one's answer moving when the other is computed.

```cpp
const ShipmentPricingService domestic(RateSettings{1.0, 2.50});
const ShipmentPricingService international(RateSettings{1.35, 9.00});

EXPECT_DOUBLE_EQ(domestic.price(100.0).total_cost, 102.50);
EXPECT_DOUBLE_EQ(international.price(100.0).total_cost, 144.0);
EXPECT_DOUBLE_EQ(domestic.price(100.0).total_cost, 102.50);  // unchanged
```

Where the singleton test proved region A's calculator changes when region B is configured, this test proves the domestic service's answer stays put no matter what the international service does, because each one owns its own `RateSettings` value instead of reading a shared global. The same file substitutes a "fake" rate for a test, and the whole substitution is a one-line value: `RateSettings{0.80, 0.0}`. No interface exists to mock, no test double needs registering, and nothing needs resetting once the test ends, because there was never anything shared to leave dirty for the next test to inherit.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain from the prerequisites; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/26-anti-patterns` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Nothing in this chapter's code depends on a C++23-only feature, so `scripts/verify.sh` discovers the identical ten gtest cases through `gtest_discover_tests` under both presets. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 10   # cpp23
100% tests passed, 0 tests failed out of 10   # cpp20
```

To see the spooky action at a distance and the isolation contrast run side by side at the terminal rather than read as an assertion, run the example's own demo script:

```sh
examples/26-anti-patterns/demo.sh cpp23
```

It prices one shipment per region through the over-patterned design, then the same two regions through the simplified design:

```
== Over-patterned (BEFORE): singleton + factory-of-factories ==
Region A quote (surcharge 1.10): $110.00
ORD-3001 priced via the manager: $110.00
ORD-3002 priced via the manager: $130.00
Region A quote, re-read with no code touching it: $130.00

== Simplified (AFTER): plain dependency injection of RateSettings ==
Domestic total (surcharge 1.00): $102.50
International total (surcharge 1.35): $144.00
Domestic total, re-read after pricing international: $102.50
```

Region A's quote moves from $110.00 to $130.00 with no code touching `region_a_calculator` directly, while the domestic total stays at $102.50 no matter what the international pricing call does. Running `examples/26-anti-patterns/demo.sh cpp20` prints identical output; nothing in this chapter's code is standard-dependent, which is itself a small instance of this chapter's lesson: a codetabs block contrasting `std::optional` against `std::expected` for `price()`'s failure path would compile under both presets, but nothing here forces that second error-reporting channel to exist, an exception already says what went wrong, and adding one anyway would be exactly the speculative generality this chapter spent its word budget warning about.

## When the simpler form is wrong too

None of this makes Singleton, Factory, or a larger orchestrating class wrong in every case, and the forces test cuts both ways. A singleton is the right call when exactly one instance must exist, that instance is effectively immutable after construction, and no caller will ever need to substitute a different one or run two configurations side by side, as with a read-only table of physical constants. `ShippingRateRegistry` fails that test on every count: it is mutated constantly, read by more than one caller, and this chapter's whole second half exists because something did need two configurations alive at once.

A factory is the right call when choosing a concrete type is a real decision, resolved by data the caller does not control at compile time: a config file naming which carrier integration to load, a registered family of handlers keyed by message type, the way a self-registering registry builds one. An interface with a second implementation already written, or a third one already scheduled, has cleared the bar this chapter's `RateCalculatorFactory` never reached.

A larger class that owns more than one responsibility is not automatically wrong either, when the responsibilities are small, stable, and have never once needed to change independently of each other in the life of the codebase. The red flag is not size, it is independent variability: the moment a logging format and a pricing rule start changing on different schedules, owned by different people, reviewed in different pull requests, the class that bundles them has started paying the god-object cost, and splitting it is worth the extra file. Until that moment arrives, splitting four cooperating lines of logic into four classes "for separation of concerns" is the same mistake run in reverse: abstraction added for a force that has not shown up yet.

The diagnostic underneath every judgment call in this section is the forces test itself: name the forces actually present in the code in front of you, not the forces a pattern's name usually implies, and let that answer decide. A pattern applied with no force present is not a lesser version of good design. It is the anti-pattern this chapter is about, wearing a familiar and defensible-looking shape.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original definition of a pattern as a recurring problem and the core of a solution to it, the same definition this book uses to distinguish a pattern applied for a reason from one applied by reflex.
- Martin Fowler, [*Refactoring: Improving the Design of Existing Code*](https://martinfowler.com/books/refactoring.html), 2nd ed. (Addison-Wesley): the code-smell catalogue this chapter's god object and speculative-generality sections draw their diagnostic language from, paired with the concrete refactorings that walk a design back from either one.
- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the Single Responsibility Principle this chapter's god-object section applies directly, and the same Dependency Inversion Principle this chapter's simplified rewrite is built on.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.

---
title: "Anti-patterns and pattern misuse"
order: 26
part: "patterns-in-the-wild"
description: "Recognizing patternitis in a shipping codebase: a singleton configuration registry holding global mutable state, a factory-of-factories wrapped around interfaces with one implementation each, a god object that validates, logs, and formats, and a leaky abstraction that exposes its own call order. The simplified rewrite collapses six types into one class and two plain values, computes the same answer, and is testable where the original was not."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Recognize a singleton used as global mutable state and show the cost it imposes.
- Recognize a god object by what it knows, not by its line count.
- Recognize speculative generality: an interface or factory with one implementation and no second in sight.
- Recognize a leaky abstraction whose public surface exposes call-order-dependent state.
- Use the forces vocabulary as a diagnostic: a pattern applied where no force calls for it is the anti-pattern, even when the code works.
- Rewrite an over-patterned design down to the shape its requirements justify, and show it's equivalent and more testable.

## Why this matters

Each pattern in the catalogue has a force that justifies it: Strategy when behavior varies by a condition a caller controls, Factory when which concrete type to build is itself a decision, Singleton when exactly one instance must exist and nothing needs to substitute another. An engineer can apply a pattern's shape from memory long after its force has stopped applying, or without checking that it ever did.

That reflex is *patternitis*: reaching for a named pattern because its shape is familiar, not because the code calls for it. It rarely shows as one wrong line. The design compiles, passes review because every piece looks textbook, and costs far more to read, test and change than the problem needs.

The problem here is a shipment price: multiply a base cost by a fuel surcharge and add a handling fee. The over-patterned version accumulates one reasonable-sounding step at a time. A configuration registry becomes a singleton because it's easy to reach from anywhere. The calculator it feeds gets built through a factory, to decouple construction from use. The factory gets handed out by a second singleton. The class that ties it together picks up logging and formatting because nothing else owned them. The result needs six types and two singletons to multiply one number by another.

{% include excalidraw.html name="26-anti-patterns" caption="Figure 26.1: the over-patterned design chains a god object through a factory-of-factories, an interface with one implementation, and a singleton holding global mutable state; the simplified design injects one plain value into one class and gets one value back." %}

| Anti-pattern | Where | Cost |
| --- | --- | --- |
| Singleton as global mutable state | `ShippingRateRegistry` | Objects that look independent change each other; two configurations can't coexist |
| Speculative generality | `RateCalculator`, `RateCalculatorFactory`, `RateCalculatorFactoryProvider` | Three hops and two singletons to reach one constructor |
| God object | `ShipmentOperationsManager` | Four unrelated reasons to change in one class |
| Leaky abstraction | `was_last_calculation_surcharge_applied()` | The answer depends on which shipment was priced last |

## Singleton as global mutable state

A correctly built Meyers singleton still carries four costs: global mutable state, a hidden dependency, an implementation that's hard to substitute, and static-initialization-order risk. `ShippingRateRegistry` in `examples/26-anti-patterns/src/shipping_rate_registry.hpp` uses a thread-safe function-local static and deleted copy operations. The mechanism is fine. The problem is what it holds: one mutable fuel surcharge that more than one part of the pricing pipeline believes it can set.

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

`StandardRateCalculator::calculate()` reads `ShippingRateRegistry::instance().surcharge_multiplier()` on every call. A region A calculator obtained at one rate reports region B's rate once someone configures region B:

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

That's *spooky action at a distance*: two objects that look independent share one mutable slot and change each other through no code path a reader can see in either. The structural problem goes deeper than the read timing. One slot can't hold two valid configurations, so a program that prices two regions at once with different surcharges can't express that through `ShippingRateRegistry` at all. A second singleton only moves the problem to a new name.

## Speculative generality: an interface with no second implementation

`RateCalculator` and `RateCalculatorFactory`, in `rate_calculator.hpp` and `rate_calculator_factory.hpp`, each have one concrete type, `StandardRateCalculator` and `StandardRateCalculatorFactory`, and no second one on the horizon. That's *speculative generality*: an abstraction built for a variation that hasn't arrived. Factory Method earns its place when choosing the type is a decision made from a config value, a runtime flag or a registered family. This factory makes no decision:

```cpp
std::unique_ptr<RateCalculator> StandardRateCalculatorFactory::create_calculator() const {
    return std::make_unique<StandardRateCalculator>();
}
```

A reader opens `RateCalculatorFactory`, confirms it has one implementation, then opens that implementation to find the logic: three hops to code that `std::make_unique<StandardRateCalculator>()` puts at the call site.

`RateCalculatorFactoryProvider` is a second singleton that hands out the one factory the program ever builds:

```cpp
RateCalculatorFactory& RateCalculatorFactoryProvider::factory() {
    return *factory_;
}
```

`RateCalculatorFactoryProvider::instance().factory().create_calculator()` goes four symbols and two singletons deep for the object `std::make_unique<StandardRateCalculator>()` returns in one line. That's a factory of factories: speculative generality compounded with Singleton's hidden dependency, each hiding the other well enough that a reviewer reading one class at a time can miss both.

<blockquote class="ai-sidebar" markdown="1">
**Specifying a simplification.** "Clean up the pricing code" doesn't say which abstractions have a force behind them, so a reply can keep the factory "in case" or add another interface.

> In `examples/26-anti-patterns`, price shipments through `ShipmentPricingService` constructed from a `RateSettings{surcharge_multiplier, handling_fee}` value, returning a `PricingResult` that carries `surcharge_applied`. Don't use `ShippingRateRegistry`, `RateCalculatorFactory` or `RateCalculatorFactoryProvider`, and add no interface, factory or singleton: there is one implementation and no second one planned. Two services with different `RateSettings` must coexist in one process, and `price()` must reject a non-positive base cost as the over-patterned design does.

Check: `ctest --preset cpp23 -R 'AntiPatternEquivalenceTest|ShipmentPricingServiceTest'` passes, and the reply's `shipment_pricing_service.hpp` contains no `virtual` and no `instance()`.
</blockquote>

## The god object: a class that knows everything

`ShipmentOperationsManager` resolves a calculator through both singletons, validates that the base cost is positive, writes an audit log entry for every shipment, and formats a receipt string. It's about 100 lines across header and source, so length isn't the test. The test is whether a reviewer can say what it does in one clause. Here it takes four:

- pricing orchestration;
- input validation;
- audit logging;
- receipt formatting.

None relates to the others. A receipt-format bug and a surcharge bug are unrelated failures in the same class, reviewed in the same diff. A pricing test has to know about `operation_log()` to see a side effect unrelated to pricing:

```cpp
EXPECT_TRUE(manager.operation_log().empty());
manager.price_shipment("ORD-3003", 50.0);
EXPECT_EQ(manager.operation_log().size(), 1U);
```

The Single Responsibility Principle, one reason to change per class, is the same diagnostic. `ShipmentOperationsManager` has four: a pricing rule, a validation rule, a log format and a receipt layout.

## Leaky abstraction: exposing what should stay hidden

`ShipmentOperationsManager::was_last_calculation_surcharge_applied()` answers a real question, whether the last calculation applied a surcharge, by keeping mutable state on the manager instead of putting the fact on the `PricedShipmentReceipt` that calculation returned:

```cpp
last_surcharge_multiplier_ = ShippingRateRegistry::instance().surcharge_multiplier();
// ...
bool ShipmentOperationsManager::was_last_calculation_surcharge_applied() const {
    return last_surcharge_multiplier_ != 1.0;
}
```

Price `ORD-3001` at the default rate and `ORD-3002` with a surcharge, and the method reports `true`: right for `ORD-3002`, wrong for anyone holding the `ORD-3001` receipt. The method answers for whichever shipment was priced last, which its name doesn't say and its signature can't express. A *leaky abstraction* is a type whose public surface fails to hide an internal detail, here that the flag is ordered state tied to the last call, so every caller must understand and track it.

## The simplified rewrite: one value, one class

`ShipmentPricingService` in `shipment_pricing_service.hpp` replaces six types across three headers, `ShippingRateRegistry`, `RateCalculator`, `StandardRateCalculator`, `RateCalculatorFactory`, `StandardRateCalculatorFactory` and `RateCalculatorFactoryProvider`, with one class and two plain values:

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

`RateSettings` is the whole configuration the six-type design existed to express, known at construction and never changed under a caller. Nothing varies polymorphically, so nothing needs an interface; a copyable value is the right abstraction. `PricingResult` fixes the leak: `surcharge_applied` travels with the `total_cost` it describes. Validation, logging and formatting each get their own small type once something gives them a reason to vary on their own: a second log format, a second validation rule, a second receipt layout.

### Same answer, better tests

Both designs, driven through the same table of base costs and surcharges, produce identical totals, and both reject a non-positive base cost with the same exception. The rewrite supports the scenario the registry's single slot can't, two configurations alive at once:

```cpp
const ShipmentPricingService domestic(RateSettings{1.0, 2.50});
const ShipmentPricingService international(RateSettings{1.35, 9.00});

EXPECT_DOUBLE_EQ(domestic.price(100.0).total_cost, 102.50);
EXPECT_DOUBLE_EQ(international.price(100.0).total_cost, 144.0);
EXPECT_DOUBLE_EQ(domestic.price(100.0).total_cost, 102.50);  // unchanged
```

Each service owns its `RateSettings`, so the domestic total doesn't move when the international one is computed. A fake rate for a test is one value, `RateSettings{0.80, 0.0}`: no interface to mock, no double to register, nothing shared to reset afterward.

## Build, run, observe

From the repository root, the superbuild picks up `examples/26-anti-patterns`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Nothing here depends on a C++23 feature, so the same 10 cases pass under both presets. The demo prices through both designs:

```sh
examples/26-anti-patterns/demo.sh cpp23
```

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

Region A's quote moves from $110.00 to $130.00 with no code touching its calculator, and the domestic total stays at $102.50. Under `cpp20` the output is identical. No `std::expected` variant of `price()` exists: an exception already says what went wrong, and a second error channel with no force behind it would be speculative generality.

One experiment shows that patching the read timing doesn't fix the singleton:

- **Capture the rate at construction.** In a scratch copy, give `StandardRateCalculator` a member initialized from `ShippingRateRegistry::instance().surcharge_multiplier()` and use it in `calculate()`. Region A now re-reads $110.00 and the retroactive-change case fails, but every calculator still takes its rate from whatever the one slot held when it was built.

## When the simpler form is wrong too

The forces test cuts both ways.

| Pattern | Right when | `ShippingRateRegistry` and friends |
| --- | --- | --- |
| Singleton | Exactly one instance, effectively immutable after construction, never substituted, never two configurations at once, as with a read-only table of physical constants | Mutated constantly, read by several callers, and two configurations were needed at once |
| Factory | Choosing the type is a real decision from data the caller doesn't control at compile time: a config file naming a carrier integration, a registered family of handlers keyed by message type, as a self-registering registry builds | One implementation, none scheduled |
| A larger class | Its responsibilities are small, stable and have never changed independently | Pricing, validation, logging and formatting change on different schedules |

Size isn't the red flag; independent variability is. Once a log format and a pricing rule change on different schedules, owned by different people and reviewed in different pull requests, splitting the class is worth the extra file. Before then, splitting four cooperating lines into four classes "for separation of concerns" is the same mistake in reverse. Name the forces in the code in front of you, not the ones a pattern's name implies, and let them decide.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original definition of a pattern as a recurring problem and the core of its solution, which separates a pattern applied for a reason from one applied by reflex.
- Martin Fowler, [*Refactoring: Improving the Design of Existing Code*](https://martinfowler.com/books/refactoring.html), 2nd ed. (Addison-Wesley): the code-smell catalogue behind the god-object and speculative-generality diagnoses, with the refactorings that walk a design back from each.
- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the Single Responsibility Principle and the Dependency Inversion Principle the rewrite is built on.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).

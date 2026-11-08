---
title: "Fowler and Feathers: contrasted and combined"
order: 29
part: "changing-inherited-code"
description: "Martin Fowler's Refactoring and Michael Feathers' Working Effectively with Legacy Code compared on an inherited shipping ETA estimator, then run in sequence: Feathers first to build a safety net, Fowler next to apply named refactorings, with every test green at each step."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- State the assumption each book makes about tests: Fowler's catalogue presumes a safety net, and Feathers' techniques build one.
- Name each method's unit of work: a named refactoring under passing tests, or a seam found and a dependency broken.
- Run the combined workflow: characterize, break a dependency with Extract Interface and Parameterize Constructor, then apply Extract Function, Replace Conditional with Polymorphism and Introduce Parameter Object.
- Explain why the two are sequential stages of one job, and what breaks if you reverse them.
- Judge whether code needs a safety net, named refactorings, or neither.

## Why this matters

Ask two engineers how to clean up tangled inherited code and you may get two moves that seem to contradict each other. One opens *Refactoring*: Extract Function here, Replace Conditional with Polymorphism there, each a small named transformation run under green tests. The other opens *Working Effectively with Legacy Code*: find a seam, break the dependency that keeps the code untestable, and pin its behavior before touching any logic. Each is right about their own book. Neither solves the other's problem worse; they solve different problems.

{% include excalidraw.html name="29-fowler-and-feathers" caption="Figure 29.1: the combined workflow as two sequential stages. Feathers characterizes the inherited code and breaks the dependency that was blocking a test, producing a safety net; Fowler's named refactorings then reshape the code underneath that safety net, with the suite green at every step in both stages." %}

Fowler starts from trust: the code has tests that run in seconds, and the catalogue teaches small, behavior-preserving moves that reshape the internals. Feathers starts from the opposite: most inherited code has no such tests, usually for structural reasons, such as a constructor that reaches for a database, a clock or a singleton, or a parameter list that makes building a fixture a research project. Feathers defines legacy code as code without tests, and his catalogue answers one question: how do you get a test around code that was never built for one, without first understanding it so well you wouldn't need one?

That starting position decides which book answers your question. Well-covered code with a tangled `if`/`else` chain needs Fowler and nothing from Feathers. A billing module with a `system_clock::now()` call buried in a two-hundred-line method needs Feathers first: no refactoring is safe without a regression test, and the one test you can write won't hold still until the clock dependency is broken. The worked example has both problems: a delivery-ETA estimator that computes transit days and a surcharge, reads the wall clock to decide on a holiday adjustment, and dispatches on carrier name through an `if`/`else` chain.

## Two books, one shared target, different starting moves

| Dimension | Fowler, *Refactoring* | Feathers, *Working Effectively with Legacy Code* |
|---|---|---|
| Starting assumption about tests | A fast, trustworthy suite exists and stays green through every step. | No such suite exists, and the code's structure is usually why. |
| Unit of work | A named refactoring, such as Extract Function, applied under the existing tests. | A seam found, one dependency broken, and a characterization test pinning current behavior. |
| Risk model | Small steps, with the tests re-run after each; the tests are the safety net. | Change as little as possible to open the first seam; current behavior, bugs included, is protected until a test says otherwise. |
| Catalogue shape | Named mechanical refactorings, each with a before and after and when it applies. | Twenty-four dependency-breaking techniques, each a minimal edit that makes one collaborator substitutable. |
| Right lens when | The code is covered and the question is how it should be shaped. | The code isn't covered and the question is what it does now. |

The risk-model row trips engineers who've read only one book. Fowler's refactorings are safe because a test catches a mistake within seconds, an asset Feathers' techniques exist to create. A refactoring applied to untested code isn't risky in itself; most are small mechanical edits. It's risky because nothing tells you if you got it wrong.

The catalogue-shape row matters too. Feathers' techniques don't improve a design. Extract Interface and Parameterize Constructor make one hard-wired collaborator substitutable and stop there. A move that broke a dependency and reorganized the logic at once would mix two kinds of risk the books keep apart.

## How they work together

Feathers runs first because it produces what Fowler's catalogue assumes. Fowler runs second because that's where the design improves. Reverse them and you remove what makes each safe.

1. **Characterize what's observable now.** Call the code with representative inputs and record what it returns as a test. The answer may be wrong; "unchanged" is the property every later step preserves.
2. **Break the one blocking dependency.** When a hidden clock, connection or singleton makes output nondeterministic, apply the minimal technique. Extract Interface gives the collaborator a substitutable type. Parameterize Constructor adds an overload that accepts it, while the original constructor keeps building the real one, so every caller compiles unchanged.
3. **Pin the newly observable behavior** with a fixed substitute value.
4. **Refactor under the tests.** Once every path is covered, apply the refactorings the code's shape calls for: Extract Function to name a buried calculation, Replace Conditional with Polymorphism to retire a type-code dispatch, Introduce Parameter Object to collapse a long parameter list. Run every test after each one. A failure means the refactoring is wrong.

The order can't run backward because a refactoring is only as safe as the tests that verify it, and tests can't verify what they can't observe. Replace Conditional with Polymorphism on a dispatch chain that reads a live clock means each run may land on a different side of a seasonal boundary, so a failure after the refactoring looks the same as a failure caused by the calendar.

<blockquote class="ai-sidebar" markdown="1">
**Specifying a cleanup.** "Clean up this legacy class" doesn't say whether behavior is pinned first, and a reply can go straight to refactorings with nothing to show they preserved behavior. State the order:

> In `examples/29-refactoring-workflow`, leave `LegacyEtaEstimator` unchanged. First pin `base_transit_days()` and `base_surcharge()` with characterization tests. Then add a `Clock` interface with `today()`, a `SystemClock` and a `FixedClock`, and a `SeamedEtaEstimator` whose no-argument constructor builds a `SystemClock` and whose `explicit` overload takes `const Clock&`; change only the clock read. Rerun the characterization values against it with a non-holiday `FixedClock`. Only then write `ModernEtaEstimator`: Extract Function for the holiday and surcharge rules, a `CarrierProfile` hierarchy chosen by `carrier_profile_for()` in place of the `if`/`else` chain, and a `ShipmentProfile` parameter object. Check that it matches `SeamedEtaEstimator` on every carrier, a July date and a December date.

Check with `ctest --preset cpp23 -R 'DependencyBreak|ModernRefactor'`, and confirm a December `FixedClock` gives 4 transit days and an $8.00 surcharge for a ground same-zone shipment.
</blockquote>

## Running one component through the full sequence

`LegacyEtaEstimator::estimate()` takes six positional arguments: a carrier name, two zone identifiers, a weight, a residential flag and a declared value. It dispatches on carrier through an `if`/`else` chain, calls two free functions for the weight, residential and value surcharges, and reads `std::chrono::system_clock::now()` to decide whether a November or December holiday adjustment applies. That last read is the dependency the sequence deals with.

### Characterize

Only part of the method can be characterized yet. `base_transit_days()` and `base_surcharge()` are pure: same inputs, same outputs. They're pinned at once:

```cpp
EXPECT_EQ(base_transit_days("ground", "A", "A", 200.0), 2);
EXPECT_EQ(base_transit_days("ground", "A", "B", 200.0), 5);
EXPECT_EQ(base_transit_days("ground", "A", "A", 1500.0), 3);  // signature day
EXPECT_DOUBLE_EQ(base_surcharge(55.0, true, 1500.0), 32.50);
```

`estimate()` as a whole isn't. Its `delivery_date` and its holiday-adjusted `transit_days` and `surcharge` depend on the day it runs, so pinning exact values would fail every November and December for no real regression. What holds on any date is an invariant: the result is the baseline or the baseline plus the fixed holiday adjustment, and the delivery date is today plus the reported transit days. That's as far as characterization reaches, and the gap shows up while writing it.

### Break the dependency

Extract Interface turns the clock read into `Clock`, an abstract type with one `today()` method. `SystemClock` is the real implementation and `FixedClock` holds "today" at a chosen date. Parameterize Constructor adds the seam: `SeamedEtaEstimator` keeps a no-argument constructor that builds a `SystemClock`, so no production caller changes, and gains an overload taking `const Clock&`.

```cpp
SeamedEtaEstimator() : owned_clock_(std::make_unique<SystemClock>()), clock_(*owned_clock_) {}
explicit SeamedEtaEstimator(const Clock& clock) : clock_(clock) {}
```

The rest of `SeamedEtaEstimator::estimate()` is identical to the legacy method; the one changed line replaces `system_clock::now()` with `clock_.today()`. With a fixed date, the holiday behavior is observable:

```cpp
const FixedClock december_clock(std::chrono::year{2026} / std::chrono::December / 15);
const SeamedEtaEstimator estimator(december_clock);
const auto result = estimator.estimate("ground", "A", "A", 10.0, false, 200.0);
EXPECT_EQ(result.transit_days, 4);        // 2 base + 2 holiday
EXPECT_DOUBLE_EQ(result.surcharge, 8.0);  // 0 base + 8 holiday
```

Every characterization value from the first step, rerun against `SeamedEtaEstimator` with a non-holiday `FixedClock`, comes out the same. That shows the dependency break touched only the line it was meant to.

### Refactor

`ModernEtaEstimator` keeps the clock seam and applies three named refactorings:

- **Extract Function** pulls the holiday and surcharge rules into `holiday_surcharge_days()`, `holiday_surcharge_amount()` and `weight_and_value_surcharge()`, each readable from its name.
- **Replace Conditional with Polymorphism** replaces the carrier chain with a `CarrierProfile` interface and one subclass per carrier, `GroundCarrierProfile`, `AirCarrierProfile` and `ExpressCarrierProfile`, selected by `carrier_profile_for()`.
- **Introduce Parameter Object** collapses the six arguments into one `ShipmentProfile`:

```cpp
struct ShipmentProfile {
    std::string carrier;
    std::string origin_zone;
    std::string destination_zone;
    double weight_kg = 0.0;
    bool is_residential = false;
    double declared_value = 0.0;
};
```

`SeamedEtaEstimator` and `ModernEtaEstimator` agree on identical inputs under identical `FixedClock`s: the non-holiday case, the holiday case, every existing carrier and the stacked-surcharge case. The refactoring changed nothing a caller can see. The polymorphic dispatch pays off next: `FreightCarrierProfile`, with a six-day base transit time, is one new class and one new line in `carrier_profile_for()`, and the other three profiles aren't edited. In the `if`/`else` version, a fourth carrier meant editing the function every other carrier lived in.

## Build, run, observe

From the repository root, the superbuild picks up `examples/29-refactoring-workflow`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 21 tests pass under both presets; no code depends on a C++23-only feature. To see the stages side by side:

```sh
examples/29-refactoring-workflow/demo.sh cpp23
```

The demo prices one ground shipment through the legacy estimator on whatever day it runs, then through the seamed and modern estimators on a July date and a December date, then prices a freight shipment the legacy design never supported. The seamed and modern stages:

```
== Stage 2: Seamed (Feathers: Extract Interface + Parameterize Constructor) ==
Ground, same zone, July 4 (no holiday adjustment): 2 day(s), surcharge $0.00, delivery 2026-07-06
Ground, same zone, December 15 (holiday adjustment): 4 day(s), surcharge $8.00, delivery 2026-12-19

== Stage 3: Modern (Fowler: Extract Function, Replace Conditional with
   Polymorphism, Introduce Parameter Object) ==
Ground, same zone, July 4: 2 day(s), surcharge $0.00, delivery 2026-07-06
Ground, same zone, December 15: 4 day(s), surcharge $8.00, delivery 2026-12-19
Freight, a carrier added after the refactor: 6 day(s), surcharge $0.00, delivery 2026-07-10
```

Stages 2 and 3 agree for every shipment that existed before the refactor. The `cpp20` demo prints the same pinned-date lines.

## When not to use it

Neither lens applies to code with no production traffic and no cost to deleting it. A characterization test pins behavior someone depends on; for code nobody calls, the fix is deletion. Check for callers and recent production use before spending a Feathers technique.

The combined workflow also stops paying when the surgery needed to reach a safety net exceeds the value of what it preserves. Feathers' minimal edits suit one or two blocking dependencies. They don't suit a component with a dozen hidden collaborators in every method, whose logic, once visible, is a few lines of business value wrapped in workarounds for systems that no longer exist. There the question becomes whether rewriting behind the current interface costs less than every seam it would take. Both books still apply: you validate a rewrite by running old and new implementations against the same characterization tests and checking they agree, the same check used between `SeamedEtaEstimator` and `ModernEtaEstimator`. Either way, the safety net comes before the structural decision, and refactoring starts only once it's green.

## For deeper coverage

- Michael Feathers, [*Working Effectively with Legacy Code*](https://www.oreilly.com/library/view/working-effectively-with/0131177052/) (Prentice Hall): the seams, the characterization-test technique, and the twenty-four dependency-breaking techniques behind the first stage.
- Martin Fowler, [*Refactoring: Improving the Design of Existing Code*](https://martinfowler.com/books/refactoring.html), 2nd ed. (Addison-Wesley): the named-refactoring catalogue, including Extract Function, Replace Conditional with Polymorphism and Introduce Parameter Object, applied in the second stage once a safety net exists.
- Hyuk Myeong, *GoogleTest in Practice* (self-published): the GoogleTest idioms behind the characterization and equivalence tests, paired with the official [GoogleTest documentation](https://google.github.io/googletest/).
- The full reading list is on the [bibliography page]({{ '/bibliography/' | relative_url }}).

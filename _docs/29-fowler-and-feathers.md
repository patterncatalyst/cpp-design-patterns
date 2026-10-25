---
title: "Fowler and Feathers: contrasted and combined"
order: 29
part: "changing-inherited-code"
description: "A methodology comparison between Martin Fowler's Refactoring and Michael Feathers' Working Effectively with Legacy Code, taught against an inherited shipping ETA estimator: a contrast table across starting test assumptions, unit of work, risk model, and catalogue shape, then the combined workflow that runs both in sequence, Feathers first to build a safety net, Fowler next to apply named refactorings, with the test suite green at every step."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- State the starting assumption each book makes about tests, Fowler's catalogue presumes a safety net already exists, Feathers' techniques exist to build one, and explain why that single difference determines which book you reach for first on a given piece of code.
- Name the unit of work each methodology operates on: a named refactoring applied under a passing suite for Fowler, a seam found and a dependency broken for Feathers, and recognize which one a piece of code in front of you needs.
- Run the combined workflow on an inherited component: write a characterization test, break a dependency with Extract Interface and Parameterize Constructor, then apply named Fowler refactorings, Extract Function, Replace Conditional with Polymorphism, Introduce Parameter Object, once the code is under test.
- Explain why the two methodologies are sequential stages of one job rather than competing schools, and what breaks if the order is reversed.
- Judge, for a given piece of inherited code, whether a safety net is the missing piece, whether named refactorings are the missing piece, or whether neither lens justifies the cost and a bounded rewrite is the sounder call.

## Why this matters

Ask two engineers how they would clean up a tangled, undocumented piece of inherited code and you will often get two different starting moves that look like they contradict each other. One reaches for *Refactoring*'s catalogue: Extract Function here, Replace Conditional with Polymorphism there, each one a small, named, behavior-preserving transformation run under a green test suite. The other reaches for *Working Effectively with Legacy Code*: find a seam, break the dependency that is keeping this code untestable, get a characterization test in place before touching a single line of logic. Both engineers are right about the book they are holding. They are wrong if either one thinks the other book is solving the same problem worse.

{% include excalidraw.html name="29-fowler-and-feathers" caption="Figure 29.1: the combined workflow as two sequential stages. Feathers characterizes the inherited code and breaks the dependency that was blocking a test, producing a safety net; Fowler's named refactorings then reshape the code underneath that safety net, with the suite green at every step in both stages." %}

Fowler's *Refactoring* opens from a position of trust: the code already has a test suite a developer can run in seconds, and the book's job is teaching a catalogue of small, named moves, each one proven to preserve behavior, that reshape the code's internal structure without changing what it does. Feathers' *Working Effectively with Legacy Code* opens from the opposite position: most inherited code has no such suite, and the reason it has none is usually structural, a hard-wired collaborator, a constructor that reaches out to a database or a clock or a singleton, a long parameter list that makes constructing a test fixture its own research project. Feathers' definition of legacy code is not "old code" or "code nobody likes." It is code without tests, and the book's entire technique catalogue exists to answer one narrow question: how do you get a test around code that was never built to be tested, without first understanding it well enough that you would not have needed the test in the first place.

That difference in starting position is not academic. It determines which book answers the question in front of you. A codebase with full test coverage and a tangled `if`/`else` chain needs Fowler's catalogue and nothing from Feathers; there is no dependency to break because nothing is blocking a test from existing today. A fifteen-year-old billing module with a direct call to `std::chrono::system_clock::now()` buried four lines into a two-hundred-line method needs Feathers first, because no named refactoring in Fowler's catalogue is safe to apply to code you cannot run a regression test against, and the one test you can write right now will not hold still long enough to be useful until that wall-clock dependency is broken. This chapter works through both halves of that claim against one piece of inherited shipping code: a delivery-ETA estimator that computes transit days and a surcharge for a shipment, reads the wall clock directly to decide whether a holiday-season adjustment applies, and dispatches on a carrier name through an `if`/`else` chain.

## Two books, one shared target, different starting moves

The contrast below is not about which book is better. It is about which forces are present in the code you are looking at, the same forces vocabulary this book has used since its opening chapters, applied here to choosing a methodology instead of choosing a pattern.

| Dimension | Fowler, *Refactoring* | Feathers, *Working Effectively with Legacy Code* |
|---|---|---|
| Starting assumption about tests | A fast, trustworthy test suite already exists and stays green through every step. | No such suite exists yet, and the code's structure is usually the reason why. |
| Unit of work | A named refactoring from a catalogue, Extract Function, Replace Conditional with Polymorphism, Introduce Parameter Object, applied under the existing suite. | A seam found, a single dependency broken, and a characterization test written to pin whatever the code currently does. |
| Risk model | Risk is managed by keeping every step small and re-running the suite after each one; the suite is the safety net. | Risk is managed by changing as little as possible to create the first seam; the code's current behavior, bugs included, is the thing being protected until a test exists to say otherwise. |
| Catalogue shape | Roughly ninety named, mechanical refactorings, each with a worked before/after and a description of when it applies. | Twenty-four dependency-breaking techniques, each a minimal, mechanical edit (adding a constructor parameter, extracting an interface, introducing a seam) aimed at making one collaborator substitutable. |
| When it is the right lens | The code already has coverage, or the question is "how should this be shaped," not "how do I know what this currently does." | The code has no coverage, and the question is "how do I find out what this currently does without trusting my own reading of it." |

Two of these rows carry the whole chapter. The risk model row is the one that trips engineers who have only read one of the two books. Fowler's refactorings are safe specifically because a test suite is there to catch a mistake within seconds; that safety is borrowed from an asset Feathers' techniques exist to create in the first place. Applying a Fowler-style refactoring to code with no tests is not wrong because the refactoring itself is risky in isolation (most of them are small, mechanical edits with a worked before/after in the catalogue), it is wrong because nothing will tell you if you got it wrong. The catalogue shape row is the other one worth sitting with: Feathers' two dozen techniques are narrower in ambition than Fowler's catalogue, by design. None of them improve a design. Extract Interface and Parameterize Constructor, the two this chapter's worked example uses, exist purely to make one hard-wired collaborator substitutable. They are not trying to make the code better to read, only testable, and that restraint is the point. A technique that both broke a dependency and reorganized the surrounding logic in one move would conflate two different kinds of risk the two books keep apart.

## How they work together

Once the contrast is named, the combined workflow follows directly from it rather than needing to be invented separately: Feathers runs first, because its job is producing the one thing Fowler's entire catalogue assumes already exists. Fowler runs second, once that safety net is in place, because its catalogue is where the actual design improvement happens. These are not two competing approaches to the same problem, chosen based on taste or team culture. They are sequential stages of one job, and reversing the order removes the thing that makes each one safe.

Concretely, the combined workflow on a single piece of inherited code looks like this. First, characterize whatever part of the current behavior is already observable without any structural change: call the code with representative inputs, record what it currently returns, and write that down as a test, not because the current answer is necessarily correct, but because "unchanged" is the property every later step needs to preserve. Second, when part of the behavior is not observable yet, usually because a hidden collaborator like a clock, a database connection, or a singleton makes the output nondeterministic or environment-dependent, apply the minimum Feathers technique that breaks that one dependency. Extract Interface gives the hidden collaborator a name and a substitutable type; Parameterize Constructor adds a new constructor overload that accepts an instance of that interface, while the original no-argument constructor keeps building the real implementation internally so every existing caller keeps compiling unchanged. Third, with the dependency broken, extend the characterization test to pin the behavior that was previously unobservable, now using a fixed, substituted value instead of whatever the real collaborator would have returned. Fourth, and only once every path through the code sits under a test, open Fowler's catalogue and apply the named refactorings the code's actual shape calls for, Extract Function to give a buried calculation a name, Replace Conditional with Polymorphism to retire a type-code dispatch, Introduce Parameter Object to collapse a parameter list that has grown past the point a reader can hold in their head. Run the full suite after each one. If it stays green, the refactoring preserved behavior. If it does not, the refactoring is wrong, not the test.

The reason this order cannot run backward is structural, not procedural. A named refactoring is only as safe as the suite that verifies it, and a suite cannot verify behavior it has no way to observe. Trying to apply Replace Conditional with Polymorphism to a carrier-dispatch chain that reads a live system clock inside the same method means every test run risks landing on a different side of a seasonal boundary, which makes a failing test after the refactoring indistinguishable from a test that was always going to fail depending on the calendar. The dependency has to be broken first, specifically because breaking it is what turns "I think this still works" into "the suite says this still works."

> **AI sidebar.** Ask an LLM pairing partner to "clean up this legacy class" and a common response jumps straight to Fowler's catalogue: Extract Function pulled out of a long method, a Strategy interface replacing a type-code switch, often well-chosen refactorings read in isolation. What that answer usually skips is the question that should come first: does a test already cover this code, and if not, what dependency is blocking one from existing? A generated refactoring applied to code with no characterization test is not wrong because the transformation is poorly chosen; it is wrong because nothing confirms the transformation preserved behavior, and a generated diff that compiles is not the same claim as a generated diff that was verified. Before accepting a suggested refactoring on inherited code, ask the pairing partner the Feathers question first: what would tell us if this change broke something, and does that thing exist yet. If the answer is "not yet," the refactoring is premature regardless of how textbook it looks.

## Running one component through the full sequence

The `LegacyEtaEstimator` this chapter works with computes a shipment's transit days and surcharge from a carrier name, two zone identifiers, a weight, a residential flag, and a declared value, six positional arguments on one `estimate()` call. Internally, the carrier dispatch runs through an `if`/`else` chain, two short free functions already compute the weight-, residential-, and value-based surcharge rules, and a final block reads `std::chrono::system_clock::now()` directly to decide whether a November-or-December holiday adjustment applies. That last line is the dependency this chapter's combined sequence exists to deal with.

The first move is characterization, and the first finding is that only part of this method's behavior is characterizable yet. `base_transit_days()` and `base_surcharge()`, the two free functions the carrier dispatch and surcharge rules live in, are already pure: same inputs, same outputs, no hidden collaborator. A characterization test pins them immediately:

```cpp
EXPECT_EQ(base_transit_days("ground", "A", "A", 200.0), 2);
EXPECT_EQ(base_transit_days("ground", "A", "B", 200.0), 5);
EXPECT_EQ(base_transit_days("ground", "A", "A", 1500.0), 3);  // signature day
EXPECT_DOUBLE_EQ(base_surcharge(55.0, true, 1500.0), 32.50);
```

`LegacyEtaEstimator::estimate()` as a whole is a different story. Its `delivery_date` and its holiday-adjusted `transit_days` and `surcharge` depend on whatever day the test happens to run, which means a characterization test cannot pin an exact value for either one without becoming a test that fails every November and December for reasons that have nothing to do with a real regression. What the characterization test can still assert is the invariant that holds regardless of the calendar: the result is either the deterministic baseline or that baseline plus the fixed holiday adjustment, and the delivery date is always today, whatever today is, plus however many transit days the result reports. That is as far as characterization alone can take this method, and that gap is the direct, concrete reason the next step exists; it is discovered by trying to write the test, not asserted as a rule in advance.

Breaking the dependency means applying Extract Interface to the wall-clock read: `Clock` is a two-method abstract type with a single `today()` method, `SystemClock` is the real implementation `estimate()` used to call inline, and `FixedClock` is a test double that holds "today" still at whatever date a test constructs it with. Parameterize Constructor adds the seam: `SeamedEtaEstimator` keeps a no-argument constructor that builds a `SystemClock` internally, so no production call site has to change, and adds a second constructor overload that takes a `const Clock&`, which is the one tests use.

```cpp
SeamedEtaEstimator() : owned_clock_(std::make_unique<SystemClock>()), clock_(*owned_clock_) {}
explicit SeamedEtaEstimator(const Clock& clock) : clock_(clock) {}
```

Everything else in `SeamedEtaEstimator::estimate()`, the carrier dispatch, the surcharge rules, is identical to `LegacyEtaEstimator::estimate()`; the only line that changed replaced `std::chrono::system_clock::now()` with `clock_.today()`. With a `FixedClock` constructed against a known date, the previously unobservable behavior becomes exactly observable:

```cpp
const FixedClock december_clock(std::chrono::year{2026} / std::chrono::December / 15);
const SeamedEtaEstimator estimator(december_clock);
const auto result = estimator.estimate("ground", "A", "A", 10.0, false, 200.0);
EXPECT_EQ(result.transit_days, 4);        // 2 base + 2 holiday
EXPECT_DOUBLE_EQ(result.surcharge, 8.0);  // 0 base + 8 holiday
```

The same test file re-runs every characterization assertion from the first step against `SeamedEtaEstimator`, with a non-holiday `FixedClock` so the seasonal adjustment never interferes, and gets identical numbers. That re-run is not a formality. It is the proof that breaking the dependency touched exactly the one line it was supposed to touch and nothing else.

Only now does Fowler's catalogue enter. `ModernEtaEstimator` keeps the clock seam from the previous stage untouched and applies three named refactorings to everything else. Extract Function pulls the holiday adjustment and the weight/residential/value surcharge rule out of `estimate()`'s body into named, single-purpose functions, `holiday_surcharge_days()`, `holiday_surcharge_amount()`, `weight_and_value_surcharge()`, each one answerable by reading its name rather than its implementation. Replace Conditional with Polymorphism retires the carrier `if`/`else` chain in favor of a `CarrierProfile` interface with one small subclass per carrier, `GroundCarrierProfile`, `AirCarrierProfile`, `ExpressCarrierProfile`, selected through `carrier_profile_for()`. Introduce Parameter Object collapses the six-argument call into one `ShipmentProfile` value:

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

The proof that all three refactorings preserved behavior is the same shape of test used after the dependency break: run `SeamedEtaEstimator` and `ModernEtaEstimator` against identical inputs under identical `FixedClock` instances and assert the results agree, for the non-holiday case, the holiday case, every pre-existing carrier, and the stacked-surcharge case. Every one of those assertions passes, which is the direct evidence that reorganizing the code's internal shape changed nothing a caller can observe. The polymorphic dispatch pays for itself immediately afterward: a fourth carrier, `FreightCarrierProfile`, returning a six-day base transit time, is added as one new class and one new line in `carrier_profile_for()`. `GroundCarrierProfile`, `AirCarrierProfile`, and `ExpressCarrierProfile` are not edited to make room for it, which is the concrete difference between this shape and the `if`/`else` chain it replaced, where adding a fourth carrier meant editing the same function every other carrier's logic already lived in.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain from the prerequisites; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/29-refactoring-workflow` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Nothing in this chapter's code depends on a C++23-only feature, so both presets discover the identical twenty-one gtest cases through `gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 21   # cpp23
100% tests passed, 0 tests failed out of 21   # cpp20
```

To see the three stages run side by side at the terminal rather than read as an assertion, run the example's own demo script:

```sh
examples/29-refactoring-workflow/demo.sh cpp23
```

It prices one ground shipment through the legacy estimator against whatever day the demo happens to run, then the same shipment through the seamed and modern estimators pinned to a non-holiday date and a holiday date, then a freight shipment the legacy design never supported:

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

Stage 2 and stage 3 produce identical numbers for every shipment that existed before the refactor, visible at the terminal rather than asserted in prose. Running `examples/29-refactoring-workflow/demo.sh cpp20` prints identical output for the pinned-date lines; nothing in this chapter's code is standard-dependent.

## When not to use it

Neither lens applies, and reaching for either one is itself a cost with no benefit purchased, when the code in question has no production traffic depending on its current behavior and no cost to deleting it outright. A characterization test exists to pin behavior someone depends on; writing one for code nobody calls anymore is ceremony around a decision that should have been "delete this file." Check for callers and for recent production use before spending a single Feathers technique on a method, because the answer is sometimes that the component is already dead and the actual fix is removal, not characterization.

The combined workflow also stops making sense once the amount of structural surgery needed to reach a safety net exceeds the value of the behavior being preserved. Feathers' techniques stay minimal by design, a constructor parameter here, an extracted interface there, and that restraint is a feature when one or two dependencies are blocking a test. It stops being a feature when a component has a dozen hidden collaborators tangled through every method, each one needing its own seam before a single characterization test can run, and the component's actual logic, once visible, turns out to be a few lines of real business value wrapped in years of accumulated workarounds for systems that no longer exist. At that point, the question worth asking is not "which dependency do I break first" but "does rewriting this component behind its current interface cost less than every seam this would take, and does writing enough characterization tests to trust a rewrite's output cost less than writing enough characterization tests to trust a refactor in place." Reaching that answer does not require abandoning either book: a rewrite's output is itself validated by running old and new implementations against the same characterization suite and confirming they agree, the same technique this chapter used to validate `SeamedEtaEstimator` against `ModernEtaEstimator`, applied to two independently written implementations instead of one refactored in stages. The decision of which path costs less belongs to its own analysis; what this chapter's combined workflow establishes is the baseline every version of that analysis needs, a safety net that exists before the structural decision gets made, and a named-refactoring stage that only touches code once that safety net is already green.

## For deeper coverage

- Michael Feathers, [*Working Effectively with Legacy Code*](https://www.oreilly.com/library/view/working-effectively-with/0131177052/) (Prentice Hall): the seams, the characterization-test technique, and the full catalogue of twenty-four dependency-breaking techniques this chapter's combined workflow draws its first stage from.
- Martin Fowler, [*Refactoring: Improving the Design of Existing Code*](https://martinfowler.com/books/refactoring.html), 2nd ed. (Addison-Wesley): the named-refactoring catalogue, Extract Function, Replace Conditional with Polymorphism, Introduce Parameter Object among roughly ninety entries, this chapter's second stage applies once a safety net exists.
- Hyuk Myeong, *GoogleTest in Practice* (self-published): the GoogleTest idioms behind every characterization and equivalence test in this chapter's example, paired with the official [GoogleTest documentation](https://google.github.io/googletest/).
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.

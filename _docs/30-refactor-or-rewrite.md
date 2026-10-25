---
title: "Refactor or rewrite"
order: 30
part: "changing-inherited-code"
description: "The decision between incremental refactoring and a bounded rewrite, taught against a legacy freight-pricing engine carrying an undocumented zone-rate matrix, two undocumented customer contract overrides, and an unexplained rounding rule. A strangler fig router replaces the legacy engine behind a stable PricingEngine interface, shifting traffic customer by customer while a shared characterization suite proves the new engine agrees with the old one before cutover."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the concrete factors that decide refactor versus rewrite: risk, cost,
  how much undocumented knowledge lives in the current code, how well that
  code is understood today, whether a safety net already exists, and what
  being wrong costs in each direction.
- Explain why "rewrite from scratch" usually loses: lost embedded knowledge,
  the dual-maintenance burden of running two systems, and a long delivery
  window with nothing shipping.
- Recognize when a bounded rewrite wins over continued refactoring, and
  state the conditions that make that call defensible rather than a reflex.
- Describe the strangler fig approach: a stable interface, a seam that
  chooses between old and new behavior at runtime, and traffic shifted
  incrementally as confidence grows.
- Build a characterization suite that both an old and a new implementation
  must pass, and use a comparison mode to catch a disagreement before a
  customer ever sees it.
- Judge when the overhead of a seam, a comparison mode, and a gradual cutover
  policy is not worth paying, and a direct in-place change or a full swap is
  the better call.

{% include excalidraw.html name="30-refactor-or-rewrite" caption="Figure 30.1: callers depend on PricingEngine, never on a concrete engine; StranglerPricingRouter sits behind that stable interface and shifts traffic from LegacyPricingEngine toward ModernPricingEngine release by release, with a compare mode running during the verification window." %}

## Why this matters

Every inherited codebase eventually produces a component nobody wants to
touch directly: a pricing engine, a tax calculator, a fraud scorer, a
billing reconciliation job. It works. It has worked for years. Nobody fully
understands why every branch in it exists, and the one person who might have
known left the company two reorganizations ago. The question that arrives at
this point is never abstract: does the team keep refactoring this component
in place, moving it toward a better design one safe step at a time, or does
it set the component aside and build a replacement from a cleaner starting
point?

Both answers are correct some of the time, which is exactly why this
decision deserves a repeatable set of factors rather than a gut call made
under deadline pressure. A team that always refactors will occasionally
spend months polishing a design that should have been replaced wholesale. A
team that always rewrites will occasionally throw away years of hard-won
edge-case handling and spend a year reimplementing it badly from memory. The
goal of this chapter is to give you the vocabulary to make that call on
purpose, with a specific technique, the strangler fig, for the case where a
bounded rewrite is the right answer and the current system cannot stop
serving traffic while the replacement gets built.

## The decision factors

Six factors drive this decision, and none of them is "how old is the code"
or "how ugly is the code," two questions that feel diagnostic and mostly are
not. Legacy code that is old and ugly but well covered by tests and fully
understood is a refactoring candidate in good standing. Code written last
quarter that nobody can explain and that has no tests at all is a rewrite
candidate despite being brand new.

**Risk.** What breaks if this component misbehaves, and who notices first?
A pricing engine that silently undercharges a customer for six months before
anyone audits the numbers carries more risk than a pricing engine that
throws a visible error the moment something is wrong. High risk argues for
the smaller, more reversible steps that refactoring and an incremental
strangler fig cutover both offer, since either one lets you stop and back
out the instant something looks wrong.

**Cost.** Refactoring in place costs engineering time spread over a long
period, with the system shipping the whole time. A rewrite costs a
concentrated block of time with a specific, nameable risk: the project runs
long, the business keeps needing changes to the old system in the meantime,
and now two systems need those changes applied twice. Cost is not just
calendar time; it is the shape of the spending.

**How much undocumented knowledge lives in the current code.** This is the
factor teams underweight most often. A freight pricing engine's zone-rate
matrix, built from years of lane negotiations nobody wrote down anywhere
else, is not incidental complexity a rewrite can skip past. It is the
business logic. Michael Feathers' name for inherited code, legacy code
defined by the absence of tests rather than by its age, matters here for a
specific reason: the same absence of tests that makes the code risky to
change is usually paired with an absence of documentation that makes its
behavior the only surviving record of what the business agreed to with a
customer years ago. A rewrite that treats that behavior as noise to clean
up instead of a requirement to preserve will ship something that compiles
and reviews well, yet quietly stops honoring a contract term nobody
remembered to mention because nobody remembered it existed outside the code.

**How well the current code is understood.** A component three engineers on
the current team can explain line by line is a different proposition from
one where "nobody really knows what this branch is for, but removing it
broke something once" is the closest thing to documentation that exists. The
less understood a component is, the more a rewrite is a bet on re-deriving
knowledge the organization already paid once to learn, and the more a
characterization suite, pinning the current behavior with tests before
changing anything, pays for itself by turning tribal knowledge into an
executable, enforceable fact.

**Whether a safety net exists.** A component backed by a thorough test suite
can be refactored aggressively, because a mistake announces itself
immediately. A component with no tests at all is not safe to refactor in any
meaningful sense: every change is a bet that nothing you cannot see depends
on the thing you just touched. Building that safety net, through
characterization tests that pin current behavior, is itself the first
concrete step regardless of which path you ultimately choose. A refactor
without a safety net is not more conservative than a rewrite; it is often
riskier, because it changes the one thing you have observed working while
having no way to detect when it stops.

**The cost of being wrong.** If refactoring turns out to be the wrong call,
the cost is wasted time and a component that is marginally better than
before. If a rewrite turns out to be the wrong call, the cost is a half-built
replacement, a legacy system that has not been touched in a year and has
drifted further from understood, and a team that has to explain to
leadership why the thing that was supposed to replace the old system does
not exist yet and the old system still cannot be touched safely either. The
two failure modes are not symmetric, and that asymmetry is itself a reason
most components should default toward refactoring unless a specific force
argues otherwise.

## Why rewrite from scratch usually loses

"Let's just rewrite it" is the single most expensive sentence available to
a team staring at an unpleasant codebase, and it loses for three concrete,
repeatable reasons rather than because rewrites are inherently bad ideas.

The first is lost embedded knowledge. A legacy pricing engine's quirks are
rarely bugs; they are fossilized decisions. A customer discount applied as a
flat dollar amount instead of a percentage, a surcharge that only applies
above a specific declared value, a rounding rule nobody can trace to a
current system, each one was put there because something, a contract, a
downstream system, a regulatory requirement, needed it at the time. A
rewrite built from a clean mental model of "how pricing should work" throws
all of that away and replaces it with how pricing should work in the
rewrite author's head, which is a different, unvalidated thing.

The second is dual maintenance. The moment a rewrite starts, the business
does not stop needing changes to the system it depends on today. Every bug
fix, every new customer contract, every regulatory change has to land in
the old system to keep the business running, and then either land again in
the half-finished rewrite or get silently forgotten, so the rewrite is
already behind before it ships. Teams routinely underestimate this cost
because it is invisible during planning and only shows up as "wait, did the
new system get the Q3 pricing change too?" six months in.

The third is the long window with no delivery. A rewrite is an all-or-
nothing bet: nothing the team builds pays off until the whole replacement
is done, tested, and cut over. A regulator, a customer, or a competitor does
not wait for that date. An incremental refactoring effort, by contrast,
ships value continuously, because every safe step is itself a small,
real improvement to the system already in production.

None of this means a bounded rewrite is always wrong. It means a rewrite
earns its cost only when the current design's problems are structural
rather than cosmetic: the language or platform itself is the limiting
factor, the architecture actively prevents a change the business needs, or
the code is so poorly understood and so sparsely tested that incremental
change is itself the riskier path. When a bounded rewrite is the right call,
it needs a way to ship without a single all-or-nothing cutover day, and that
is what the strangler fig approach provides.

## The strangler fig approach

Martin Fowler borrowed the name from a tree, the strangler fig, that
germinates in the canopy of a host tree and sends roots down to the ground,
gradually enveloping the host until the host can be removed and the fig
stands on its own. The software version replaces this: a legacy component
behind a stable interface, incrementally, routing more traffic to the new
implementation as confidence grows, with the old implementation removable
only once the new one has proven itself against the same bar the old one
was held to.

Three pieces make this work, and each one exists for a specific reason
rather than being extra ceremony layered onto a simple problem.

**A stable interface.** Every caller depends on an abstraction, `quote()`
in this chapter's worked example, never on the concrete legacy or modern
implementation. This is the same force that justifies dependency inversion
anywhere else in this book: callers are insulated from which concrete type
answers them, so the implementation behind the interface can change without
touching a single call site.

**A routing seam.** Something has to decide, for a given request, whether
the legacy implementation, the new implementation, or both should handle
it. That seam can run three distinct modes, each earning its keep at a
different stage of the migration: legacy-only as the safe starting point
and rollback target, a comparison mode that runs both implementations and
logs where they disagree without ever letting the new implementation's
answer reach a customer, and a routed mode that sends specific traffic,
often a specific customer, tenant, or percentage, to the new
implementation while everyone else still gets the old one.

**A shared characterization suite.** The new implementation does not earn
the right to take traffic by looking cleaner. It earns that right by
passing every case in the same suite that pins the old implementation's
behavior, including the cases that encode undocumented business rules
nobody wrote down anywhere else. Passing a different, newly written test
suite proves the new code does what its author intended; passing the old
suite proves it does what the business needs, which is a different and
much stronger claim.

The traffic shift itself is the part that makes this a strangler fig
specifically, rather than just "build a facade." The old implementation's
share of traffic does not drop to zero in one release. It shrinks: one
customer, one region, or one percentage point at a time, with each
increment observable and reversible if something looks wrong.
The host tree, the legacy component, keeps doing its job for the traffic
still routed to it right up until the day the last sliver of that traffic
moves, at which point the legacy component can be deleted with nobody
noticing the difference in production, because the difference was already
proven to not exist.

## The migration, worked in code

`PricingEngine` is this example's stable interface: one method, `quote()`,
that every concrete engine implements. `LegacyPricingEngine` is the
component under review. Its zone-rate matrix prices a lane between five
regional hubs, grown by hand across years of negotiations with no formula
backing it:

{% raw %}
```cpp
const std::map<std::pair<std::string, std::string>, int>& zone_matrix() {
    static const std::map<std::pair<std::string, std::string>, int> table{
        {{"NE", "NE"}, 400},  {{"MW", "MW"}, 350},  {{"SE", "SE"}, 300},
        // ... eleven more lanes, none of them a formula
    };
    return table;
}
```
{% endraw %}

Two customer overrides sit beside it, each a one-off branch rather than a
named, shared discount mechanism:

```cpp
if (customer_id == "ACME-2019" && subtotal_cents > 2000) {
    return subtotal_cents - 500;
}
if (customer_id == "GLOBEX") {
    return static_cast<int>(subtotal_cents * 0.90);
}
```

Nothing marks either branch as implementing a negotiated contract term. A
reader has to already know ACME-2019's volume rebate exists to recognize
this code as the thing that enforces it, which is exactly the kind of
undocumented knowledge this chapter's decision factors warn against
discarding. A final step, rounding every total up to the nearest quarter
dollar, is inherited from a decommissioned mainframe invoicing system; no
one currently on the team can confirm whether the system that consumed
quarter-rounded totals is still in service, so the rewrite keeps the rule
rather than guessing it is safe to drop.

`tests/pricing_characterization_cases.hpp` pins all of this with seven
shipment profiles and the exact totals `LegacyPricingEngine` returns today,
captured by running the engine, not derived from a specification that may
not exist. `ModernPricingEngine` is judged against that same table, not
against a fresh set of expectations written while rebuilding the class:

```cpp
TEST(ModernPricingEngineTest, MatchesEveryCharacterizationCase) {
    ModernPricingEngine engine;
    for (const auto& test_case : pricing_characterization_cases()) {
        EXPECT_EQ(engine.quote(test_case.request).total_cents,
                   test_case.expected_total_cents)
            << "case: " << test_case.name;
    }
}
```

`ModernPricingEngine` restructures the same knowledge rather than
discarding it: a named `zone_rate_card()` in place of an inline matrix, a
`CustomerAdjustment` struct and lookup table in place of the if/else chain,
and the quarter-dollar rounding rule kept intact. The rewrite's value is in
the shape of the code, not in any claim that the pricing rules themselves
were wrong and needed correcting; `tests/legacy_modern_agreement_test.cpp`
drives both engines through the identical table and asserts their totals
match, a direct engine-to-engine comparison independent of either one's own
pinned expectations.

`StranglerPricingRouter` is the seam. `Mode::Compare` runs both engines on
every request and records any disagreement, while still returning the
legacy engine's answer:

```cpp
case Mode::Compare: {
    const Quote legacy_quote = legacy_->quote(request);
    const Quote modern_quote = modern_->quote(request);
    if (legacy_quote.total_cents != modern_quote.total_cents) {
        disagreements_.push_back(QuoteDisagreement{request, legacy_quote, modern_quote});
    }
    return legacy_quote;
}
```

`tests/strangler_pricing_router_test.cpp` proves this mode does its job
with a deliberately buggy stand-in for the modern engine: the router
detects the disagreement and still hands the customer the legacy answer,
proving a defect introduced during the rewrite cannot reach a customer
during the verification window, which is the entire reason to run a
comparison mode at all rather than cutting straight over. Once the real
`ModernPricingEngine` runs through the same comparison with zero
disagreements across the characterization table, `CutoverPolicy` starts
moving named customers over one at a time in `Mode::Routed`:

```cpp
EngineChoice CutoverPolicy::choose(const ShipmentRequest& request) const {
    return is_migrated(request.customer_id) ? EngineChoice::Modern : EngineChoice::Legacy;
}
```

The demo program plays this out as five releases: a cold `LegacyOnly`
start, a `Compare` release that finds zero disagreements, a `Routed`
release that migrates one customer while the rest stay on legacy, a
`Routed` release where every customer has moved, and a final `ModernOnly`
release where `LegacyPricingEngine` could be deleted without anyone
noticing a difference. Every quote printed across all five releases is
identical, which is the strangler fig's actual promise: the migration is
invisible to the customer because the characterization table and the
agreement test proved it would be before a single real request was ever
routed to the new engine.

## A stable interface's failure path: C++20 versus C++23

`PricingEngine::quote()` throws `std::invalid_argument` for a negative
weight or an unrecognized zone pair, a contract every concrete engine
honors identically whether the interface was compiled under C++20 or
C++23. A caller written against this interface in C++20 has two
conventional ways to turn that exception into an explicit result instead:
catch it directly, or wrap the call in a helper that reports success or
failure through a sentinel, losing the specific reason for the rejection.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: a caller-side helper loses the reason for rejection; it learns
// *that* a request was invalid, not *why*, unless it also catches the
// exception directly.
std::optional<strangler30::Quote> try_quote_cpp20(const strangler30::PricingEngine& engine,
                                                   const strangler30::ShipmentRequest& request) {
    try {
        return engine.quote(request);
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}
```
```cpp
// C++23: PricingEngine::try_quote() is implemented once, in the base
// class, and returns a typed error every caller can branch on without
// touching an exception at the call site.
std::expected<Quote, PricingError> try_quote(const ShipmentRequest& request) const {
    if (request.weight_kg < 0.0) {
        return std::unexpected(PricingError::NegativeWeight);
    }
    try {
        return quote(request);
    } catch (const std::invalid_argument&) {
        return std::unexpected(PricingError::UnknownZonePair);
    }
}
```

What makes this pairing worth showing is what does not change: neither
`LegacyPricingEngine` nor `ModernPricingEngine` knows `std::expected`
exists, because `try_quote()` is implemented once in `PricingEngine` using
the non-virtual interface idiom and calls the same `quote()` override
every concrete engine already provides. That is the stable interface
doing its job in a second, smaller way: the caller-facing contract gained a
typed failure path between standards, and neither the legacy component nor
its replacement needed to change at all to support it.

> **AI sidebar.** Ask an LLM pairing partner to "modernize this legacy
> pricing engine" and a common first answer is a full rewrite: a clean
> class hierarchy, a tidy configuration object, maybe a template for
> flexibility, built from whatever the model infers the pricing rules
> ought to be. What it will not do on its own is go looking for the
> specific undocumented knowledge, a customer's negotiated flat discount,
> a rounding rule tied to a system that might not even exist anymore,
> sitting inside the branches it is about to delete. A generated rewrite
> is judged by whether it compiles and reads cleanly, not by whether it
> still honors a contract term that exists nowhere except in the code
> being replaced. Before accepting a generated rewrite of anything that
> has been in production for years, ask the same question this chapter's
> decision factors ask: what does the current code know that is not
> written down anywhere else, and has anything been built to prove the
> replacement still knows it? A characterization suite pulled from the
> running legacy system, not written from a specification, is the only
> artifact that answers that question with evidence instead of
> confidence.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan
toolchain from the prerequisites; there is no container anywhere in this
loop. From the repository root, the root superbuild picks up
`examples/30-refactor-or-rewrite` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`PricingEngine::try_quote()` only exists under C++23, so the two presets
discover a different number of gtest cases through `gtest_discover_tests`.
Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 14   # cpp23
100% tests passed, 0 tests failed out of 12   # cpp20
```

To see the migration play out rather than read about it, run the
example's own demo script, which steps through five releases of the same
cutover from a cold `LegacyOnly` start to a `ModernOnly` finish:

```sh
examples/30-refactor-or-rewrite/demo.sh cpp23
```

```
== Release 1: LegacyOnly, nothing has moved yet ==
ACME-2019 quote: $16.00
GLOBEX quote: $5.00
WIDGETCO quote: $68.75

== Release 2: Compare mode, modern runs silently alongside legacy ==
disagreements found: 0 (0 means modern is safe to start taking traffic)

== Release 3: Routed, ACME-2019 migrated, GLOBEX and WIDGETCO still legacy ==
ACME-2019 quote (now via modern): $16.00
GLOBEX quote (still via legacy): $5.00
customers migrated so far: 1 of 3

== Release 4: Routed, every customer migrated ==
GLOBEX quote (now via modern): $5.00
WIDGETCO quote (now via modern): $68.75
customers migrated so far: 3 of 3

== Release 5: ModernOnly, LegacyPricingEngine can be deleted ==
ACME-2019 quote: $16.00
GLOBEX quote: $5.00
WIDGETCO quote: $68.75
```

Every total is identical from the first release to the last. That is the
strangler fig's entire argument made visible at a terminal instead of
asserted in prose: the cutover is invisible to a paying customer because
the characterization table and the comparison mode proved it would be
before `StranglerPricingRouter` ever routed a single real request to
`ModernPricingEngine`. Running `examples/30-refactor-or-rewrite/demo.sh
cpp20` prints identical output; nothing in the demo path depends on a
C++23-only feature.

## When not to use it

A strangler fig migration is not free, and the decision factors that
justify a bounded rewrite in the first place cut against the seam itself
once the circumstances change.

A component with low risk, low cost, and a codebase the team already
understands does not need a router, a comparison mode, or a gradual
cutover policy at all. If a bug in the replacement would be caught in code
review and fixed in an afternoon, build the replacement, run it against a
characterization suite once, and cut over directly. The entire apparatus
this chapter builds exists to make a mistake observable and reversible
before it reaches a customer; paying that cost for a change nobody would
notice if it went briefly wrong is solving a problem that was never
present.

A component with no meaningful traffic segmentation, no way to identify
"this customer's request" versus "that customer's request" at the point
where routing would happen, cannot support a gradual, routed cutover in
any case that matters. `CutoverPolicy` works here because a shipment
request carries a `customer_id` the router can key on. A system that
processes anonymous, unsegmented traffic can still run a comparison mode,
but the gradual, traffic-shifted cutover this chapter spends most of its
space on collapses to a single switch: compare until confident, then flip
everyone at once. That is a legitimate simplification, not a failure to
apply the pattern correctly.

A small or short-lived component does not justify building a
second, parallel implementation at all. If the component in question is
fifty lines, fully covered by a test suite that already exists, and
understood by everyone who touches it, that is squarely a refactoring
candidate, not a rewrite candidate, and introducing `PricingEngine`-style
interface indirection purely so two implementations can coexist is
speculative generality in exactly the sense this book's anti-patterns
chapter describes: an abstraction built for a variation, a second
implementation, that the actual situation does not call for.

Finally, do not reach for a strangler fig when the real blocker is that
nobody has built the safety net yet. If no characterization tests exist
for the legacy component, building the seam before pinning current
behavior only gives a new implementation a routing mechanism with nothing
yet proven for it to agree with. The decision factors in this chapter's
opening section, not the seam itself, are the first checkpoint: establish
risk, cost, how much undocumented knowledge is in play, and whether
a safety net exists, and let those answers decide whether this chapter's
machinery is worth building at all.

## For deeper coverage

- Michael Feathers, [*Working Effectively with Legacy Code*](https://www.oreilly.com/library/view/working-effectively-with/0131177052/) (Prentice
  Hall): the definition of legacy code as code without tests, the full
  catalogue of seams and dependency-breaking techniques this book's
  inherited-code part modernizes for C++20/23, and the case for
  characterization tests as the first step before any change.
- Martin Fowler, [*Refactoring: Improving the Design of Existing Code*](https://martinfowler.com/books/refactoring.html), 2nd
  ed. (Addison-Wesley): the refactoring catalogue and the discipline of
  small, behavior-preserving steps that this chapter's "usually refactor"
  default rests on.
- Martin Fowler, ["StranglerFigApplication"](https://martinfowler.com/bliki/StranglerFigApplication.html): the original
  description of the strangler fig technique this chapter's worked example
  builds, including the naming history behind the metaphor.
- The full reading list behind this book's opinions lives on the
  [bibliography page]({{ '/bibliography/' | relative_url }}); each
  citation there is a pointer to the full book, not a substitute for
  reading it.

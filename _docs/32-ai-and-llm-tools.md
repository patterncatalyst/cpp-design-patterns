---
title: "Working with AI and LLM tools"
order: 32
part: "putting-it-together"
description: "Using an LLM as a pattern-literate collaborator: name the forces and the candidate pattern, recognize over-patterning, hallucinated APIs and outdated C++, and gate every draft on behavior tests plus an idiomatic review. A usage-tier Strategy pairs a correct classifier with a plausible one carrying a single off-by-one at a billing boundary, caught by the same table run against both."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Prompt with the forces and a candidate pattern name instead of a vague feature request.
- Recognize three failure modes of generated C++: a pattern no force calls for, an API that doesn't exist, and code that predates RAII, move semantics or the project's standard.
- Gate every generated pattern on a behavior test table and an idiomatic review, neither satisfied by the code compiling.
- Use an LLM to draft characterization tests and propose seams, while keeping the decision about which seam is safe.
- Find the one comparison that separates a correct Strategy from a plausible wrong one, and the table row that catches it.
- Decide when asking an LLM costs more than it returns.

{% include excalidraw.html name="32-ai-and-llm-tools" caption="Figure 32.1: the engineer names the forces and a candidate pattern, the AI drafts an implementation, and nothing ships past the verification gate, behavior tests plus an idiomatic-review pass, without a pass on both." %}

## Why this matters

An LLM can draft a Strategy, a Factory Method or a dependency-breaking seam in seconds, in a shipping system's own vocabulary if you supply it. That speeds up the step from design decision to running code. It doesn't change who answers for the code once it ships.

Treat the model as a collaborator who has read every pattern book and applies none of the judgment about when a pattern belongs. Its answers are fast and plausible, and plausibility is loosely related to correctness, idiom or proportion. A vague prompt, "make this configurable", gets a vague, usually over-engineered answer, because the shape of the question is all the model has to anchor on. A precise prompt names the varying behavior, the caller who controls it and the candidate pattern, and the answer can be judged by the usual forces test: does the code need this, and does the reply solve it the way the forces call for?

AI changes who writes the code, not who is responsible for whether it's correct and idiomatic.

## Naming the forces: prompting as a shared vocabulary

The forces vocabulary is the contract between engineer and model. Compare two requests:

| Prompt | What the model gets |
| --- | --- |
| "Write me a function that calculates shipping cost differently per carrier." | Nothing about whether the carrier set is open, how often a carrier is added, or what else must be decoupled. It fills each gap with whatever shape is common for that phrasing, which is rarely the minimal one. |
| "The fuel surcharge calculation varies by carrier contract, and the rest of checkout shouldn't know which carrier's rule is active. Write it as a Strategy with the pattern's usual shape." | The force (behavior varies independently of its caller), the pattern to anchor on (an interface and an injected implementation, not a conditional chain), and a scope boundary (nothing outside the varying behavior changes). |

Each constraint narrows the space of plausible answers. Naming the candidate pattern isn't a courtesy to the model; it turns an underspecified request into a specified one.

The vocabulary works on the reply too. A generated `ShippingStrategyFactory` that returns one of three hard-coded types from an enum, with no injected dependency and no extension point, isn't a Factory Method; it's a switch statement with a factory's name. Ask where the decision it encapsulates is made. If it's the enum value chosen at the call site, the factory wraps a decision already made, and a direct constructor call per branch is what the forces support.

Naming patterns also rules them out: "This value is read in one place, set once at startup, and nothing needs a second source for it, so don't wrap it behind an interface." That instruction heads off the most common failure mode.

## The failure modes of an unchecked collaborator

All three come from a model producing a plausible answer to the prompt rather than the smallest correct answer to the problem.

**Over-patterning a small problem.** Ask for a function that reads one boolean from a JSON config and you can get a `ConfigProvider` interface, a `JsonConfigProvider`, a `ConfigProviderFactory` that returns it, and a `ConfigSingleton` holding the instance. Each piece has a textbook justification, and none answers a force present in reading one boolean from one file. This is patternitis, a pattern applied because its shape is familiar, and a model trained on over-engineered example code reproduces it whenever a prompt leaves room. For every type the reply introduces, ask what force justifies it, and delete the ones with no answer.

**Hallucinated APIs.** A model writes syntactically valid calls to standard library functions that don't exist, passes arguments in an order the real function never took, or uses a member from a different version of a library than the one pinned. It generates the tokens that look most like correct code, with no compiler in the loop. A `std::ranges` call with another overload's signature, or a constructor matching an older major version than `conanfile.py` pins, reads as plausible and fails when compiled, or compiles against a similar overload with different behavior and fails at run time. Compile it, and check the header or the documentation rather than the model's description of the API.

**Non-idiomatic or outdated C++.** Training data spans every standard since C++98, and the model has no preference for the one the project targets. The reply shows `new` and `delete` where RAII belongs, raw pointers from factories where `std::unique_ptr` states ownership, hand-written loops where a range-`for` or a `std::ranges` algorithm is clearer, `typedef` instead of `using`, and status codes through output parameters instead of `std::optional` or `std::expected`. It compiles and works, and it doesn't look like the codebase it joins. Accepting it because it compiles and passes erodes the project's baseline one function at a time.

## The verification discipline that makes AI output safe

None of the three is visible to a skim. An over-patterned reader compiles. A hallucinated API sometimes binds to the wrong overload and survives a smoke test. Non-idiomatic code compiles by definition. What catches all three, whoever wrote the code, is a behavior test that proves the code does what it claims, not that the binary exits zero.

Two gates stand between a generated pattern and a merge, and both are required:

| Gate | What it checks | What it catches |
| --- | --- | --- |
| **Behavior tests** | The code against a specification a human wrote and understood | Wrong answers, including at boundaries |
| **Idiomatic review** | The code against the project's conventions: RAII, `std::expected` or `std::optional` over output parameters, concepts over unconstrained templates | Code that works but doesn't fit, which no compiler flags |

Write or review the table yourself. Tests produced by asking the same model to test what it just wrote inherit its blind spots: if it read a tier boundary as exclusive, its tests usually expect the exclusive answer. A pattern that passes its tests but reads like an older standard than the rest of the file has failed half the gate.

The model is useful on the other side of the gate too. Characterization tests, which pin an existing system's behavior before a change, are mechanical, exhaustive work a model does well: "generate a test for every branch this function takes, asserting what it returns now, without changing it" yields broad coverage faster than typing the cases. Asking where a function's dependency on a concrete type could be broken without changing behavior produces candidate seams worth evaluating. Whether a seam is safe to cut in a system with side effects or shared state the model can't see is a decision for the engineer who knows the blast radius.

## Catching the bug in code: a usage-tier Strategy

`examples/32-ai-and-llm-tools` builds the case the gate exists for. The prompt is forces-based: "a metered API gateway classifies a customer's monthly request count into one of three billing tiers, Starter, Growth or Scale, and the rule varies by nothing but that count, so write it as a Strategy with the usual shape, an abstract interface and a concrete classifier behind it." `UsageTierStrategy` (`src/usage_tier_strategy.hpp`) is the interface: one virtual method, `Classify(long monthly_requests)`, returning a `UsageTier`.

`CorrectUsageTierStrategy` follows the pricing spec: Starter below 10,000 requests a month, Growth from 10,000 to 99,999, Scale from 100,000. Both lower bounds are inclusive, so the 10,000th request moves a customer into Growth.

```cpp
UsageTier CorrectUsageTierStrategy::Classify(long monthly_requests) const {
    if (monthly_requests < 10'000) {
        return UsageTier::Starter;
    }
    if (monthly_requests < 100'000) {
        return UsageTier::Growth;
    }
    return UsageTier::Scale;
}
```

`PlausibleWrongUsageTierStrategy` is what an AI pairing partner returned for the same prompt: the same interface, tiers and early returns a reviewer skimming for "does this look like a Strategy" would approve. It has one bug, `<=` instead of `<` on the Starter boundary:

```cpp
UsageTier PlausibleWrongUsageTierStrategy::Classify(long monthly_requests) const {
    if (monthly_requests <= 10'000) {  // off-by-one: should be < 10'000
        return UsageTier::Starter;
    }
    if (monthly_requests < 100'000) {
        return UsageTier::Growth;
    }
    return UsageTier::Scale;
}
```

Side by side, the bug is easy to miss. The structure, branch count and second comparison are identical, and `<=` reads as reasonable for an inclusive-sounding tier unless you're holding the spec's wording. It's syntactically clean, structurally idiomatic, and wrong at one input.

The gate is one shared table, run against any strategy, covering each tier's low end, middle and high end:

```cpp
void ExpectSpecCompliantClassification(const UsageTierStrategy& strategy) {
    EXPECT_EQ(strategy.Classify(0), UsageTier::Starter);
    EXPECT_EQ(strategy.Classify(9'999), UsageTier::Starter);
    EXPECT_EQ(strategy.Classify(10'000), UsageTier::Growth);
    EXPECT_EQ(strategy.Classify(50'000), UsageTier::Growth);
    EXPECT_EQ(strategy.Classify(99'999), UsageTier::Growth);
    EXPECT_EQ(strategy.Classify(100'000), UsageTier::Scale);
    EXPECT_EQ(strategy.Classify(250'000), UsageTier::Scale);
}
```

`CorrectUsageTierStrategy` passes every row. `PlausibleWrongUsageTierStrategy` fails exactly one, the 10,000-request row; every other row matches. That's why a one-operator bug slips past reading and can't slip past a table that tests every boundary instead of one value inside each tier. GoogleTest's failure-inspection macro demonstrates the catch without turning the build red, by requiring exactly one failure that mentions `10'000`:

```cpp
    EXPECT_NONFATAL_FAILURE(ExpectSpecCompliantClassification(strategy), "10'000");
```

The bug has a price. At exactly 10,000 requests, Growth bills a flat $99.00; the wrong classification keeps the customer on Starter's $0.02 per request, a $200.00 bill and a $101.00 overcharge from one comparison operator. Nothing about a three-branch Strategy looks risky enough to skip testing. The risk was never the pattern's complexity; it was the boundary.

<blockquote class="ai-sidebar" markdown="1">
**Specifying a Strategy.** "Write a tier classifier" doesn't say which way each boundary falls, and a reply can choose `<=` where the spec means `<`. State the boundaries and the gate:

> In `examples/32-ai-and-llm-tools`, implement `CorrectUsageTierStrategy::Classify(long monthly_requests)` against the `UsageTierStrategy` interface. Return `UsageTier::Starter` below 10,000, `UsageTier::Growth` from 10,000 through 99,999, and `UsageTier::Scale` from 100,000; both lower bounds are inclusive. Use early returns with `<` comparisons only. Don't change `ExpectSpecCompliantClassification` or `MonthlyBillUsd`. At exactly 10,000 requests the bill must be $99.00.

Check with `ctest --preset cpp23 -R 'UsageTierClassification|MonthlyBillUsd'`; all six pass only if both boundaries are inclusive.
</blockquote>

## Build, run, observe

From the repository root, the superbuild picks up `examples/32-ai-and-llm-tools`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 6 tests pass under both presets; no code depends on a C++23-only feature, so there's no codetabs contrast. To see both strategies classify the same counts and the billing gap at the boundary:

```sh
examples/32-ai-and-llm-tools/demo.sh cpp23
```

```
== CorrectUsageTierStrategy: lower tier boundary is inclusive ==
      9999 requests -> Starter ($199.98/mo)
     10000 requests -> Growth  ($99.00/mo)
     50000 requests -> Growth  ($99.00/mo)
     99999 requests -> Growth  ($99.00/mo)
    100000 requests -> Scale   ($499.00/mo)

== PlausibleWrongUsageTierStrategy: AI-drafted, looks identical on read ==
      9999 requests -> Starter ($199.98/mo)
     10000 requests -> Starter ($200.00/mo)
     50000 requests -> Growth  ($99.00/mo)
     99999 requests -> Growth  ($99.00/mo)
    100000 requests -> Scale   ($499.00/mo)

== The boundary a read-only review missed ==
At exactly 10,000 requests:
  correct classification: Growth ($99.00/mo)
  plausible-wrong classification: Starter ($200.00/mo)
  billing error from one comparison operator: $101.00
```

The two strategies differ on one line. The `cpp20` demo prints the same output.

One experiment, in a scratch copy:

- Change `<=` to `<` in `PlausibleWrongUsageTierStrategy::Classify`: the failure-inspection case fails, because the shared table no longer produces the one failure it expects.

## When not to use it

An LLM pays off on a task with a clear specification and a cheap way to check the answer: a well-understood contract, a test table that exists or can be written, a refactor whose behavior preservation can be shown mechanically. It costs more than it returns here:

| Situation | Why |
| --- | --- |
| The specification is the hard part | A fluent answer to an unclear requirement looks finished, which makes a plausible wrong answer more dangerous than an obviously incomplete one. |
| The change touches shared mutable state, concurrency or an invariant across files the model wasn't shown | Its sense of "safe" is bounded by its context window, not by what the system guarantees. |
| The bottleneck is a design decision: which pattern, whether an abstraction is premature, whether a class has outgrown its responsibility | Delegating it moves the judgment somewhere less equipped, and you re-derive the forces analysis to check it anyway. |

The calls that stay with the engineer don't change as the tooling improves: naming the forces in the actual problem, deciding whether a pattern's cost is worth its benefit, and answering for the result once it ships. A model can draft any pattern and its tests in seconds. It can't tell you whether the problem needed a pattern, and it has no stake in being right.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480), Addison-Wesley: the shared pattern vocabulary that serves as the interface between an engineer and an AI pairing partner, and the catalogue a model's over-patterning misapplies.
- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445), Prentice Hall: the Single Responsibility and Dependency Inversion reasoning behind the idiomatic-review gate.
- Michael Feathers, [*Working Effectively with Legacy Code*](https://www.oreilly.com/library/view/working-effectively-with/0131177052/), Prentice Hall: the characterization-test technique and seam-finding discipline handed to an AI pairing partner for the mechanical half of legacy work, with the choice of seam kept by the engineer.
- Hyuk Myeong, *GoogleTest in Practice*, self-published: the GoogleTest idioms behind the behavior-test gate, read alongside the official [GoogleTest documentation](https://google.github.io/googletest/).

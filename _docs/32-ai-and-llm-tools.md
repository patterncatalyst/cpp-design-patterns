---
title: "Working with AI and LLM tools"
order: 32
part: "putting-it-together"
description: "Using an LLM as a pattern-literate collaborator rather than a code vending machine: naming the forces and the candidate pattern to get useful output, the failure modes an unchecked collaborator produces (over-patterning a three-line problem, hallucinated APIs, non-idiomatic C++), and the verification discipline, behavior tests plus an idiomatic-review pass, that decides whether a draft ships. A runnable usage-tier Strategy pairs a correct implementation against an AI-plausible one carrying a single off-by-one at a billing-tier boundary, caught by the identical behavior table run against both."
duration: "35 min"
---

## Learning objectives

By the end of this chapter you can:

- Prompt an LLM pairing partner with the forces vocabulary and a candidate pattern name instead of a vague feature request, and explain why that framing changes the quality of what comes back.
- Recognize the three recurring failure modes of AI-generated C++: reaching for a pattern no force calls for, inventing an API that does not exist, and writing code that predates RAII, move semantics, or the standard the project targets.
- State the verification discipline that gates every AI-suggested pattern before it ships: a behavior test suite and an idiomatic-review pass, neither one optional and neither one satisfied by the code merely compiling.
- Use an LLM for legacy-code work specifically: drafting characterization tests against existing behavior and proposing seams, without letting it decide which seam is safe to cut.
- Read a correct Strategy implementation against an AI-plausible, subtly wrong one and say exactly which comparison produces the difference, then show the single test that catches it.
- Decide when asking an LLM costs more than it returns, and name the judgment calls in this book that stay with the engineer regardless of how good the tooling gets.

{% include excalidraw.html name="32-ai-and-llm-tools" caption="Figure 32.1: the engineer names the forces and a candidate pattern, the AI drafts an implementation, and nothing ships past the verification gate, behavior tests plus an idiomatic-review pass, without a pass on both." %}

## Why this matters

An LLM pairing partner can draft a Strategy, a Factory Method, or a dependency-breaking seam in seconds, and it can do it in a shipping system's actual vocabulary if you give it that vocabulary to work with. That capability changes how fast a design decision turns into running code. It does not change who answers for the code once it ships. The forces vocabulary and pattern catalogue this book builds, Strategy when behavior varies by a condition the caller controls, Factory Method when choosing a concrete type is itself a decision, a seam when an existing dependency needs to move before a test can reach it, were never only for a human reviewer reading someone else's pull request. They are also the most reliable way to get useful work out of a model that has seen a vast amount of C++ in its training data and no idea which three lines of that corpus apply to the function in front of you right now.

Treat an LLM as a collaborator who has read every pattern book ever indexed and applied none of the judgment that decides when a pattern belongs. That collaborator is extraordinarily fast at producing a plausible-looking answer to whatever question you ask, and the plausibility of the answer has only a loose relationship to whether it is correct, idiomatic, or proportionate to the problem. A vague prompt, "make this configurable" or "add some error handling," gets a vague and usually over-engineered answer, because the model has nothing to anchor the shape of its response except the shape of the question. A precise prompt, naming the varying behavior, the caller who controls it, and the candidate pattern you expect to resolve it, gets an answer you can evaluate against the same forces test this book applies everywhere else: does the code in front of you need this, and does what came back solve it the way the forces call for.

The throughline for this chapter, and for using AI tooling on any codebase that has to run in production, is a single sentence: AI changes who writes the code, not who is responsible for whether it is correct and idiomatic. Everything else in this chapter is a consequence of taking that sentence seriously.

## Naming the forces: prompting as a shared vocabulary

The forces vocabulary is the interface between an engineer and an LLM in the same way an API contract is the interface between two services. A request phrased as "the fuel surcharge calculation needs to vary by carrier contract, and the rest of checkout should not need to know which carrier's rule is active, write this as a Strategy with the pattern's usual shape" gives the model three things a vague request withholds: the force that justifies a pattern at all (the behavior varies independently of its caller), the pattern name to anchor its answer against (so it reaches for an interface and an injected implementation rather than a chain of conditionals), and an implicit scope boundary (nothing outside the varying behavior needs to change). Each of those three constraints narrows the space of plausible answers the model can produce, and a narrower space of plausible answers is a more reliable space.

Compare that to "write me a function that calculates shipping cost differently per carrier." Nothing in that sentence tells the model whether the carrier set is closed or open, whether a new carrier is a common event or a once-a-year one, or whether anything else in the system needs to be decoupled from the choice. The model fills every one of those gaps with a guess drawn from whatever shape is statistically common in its training data for "calculate X differently per Y," and the shape that is statistically common for that phrasing in a large C++ corpus is rarely the minimal one. Naming the candidate pattern is not a courtesy to the model. It is the mechanism that turns an underspecified request into a specified one, the same discipline this book's entire first part applies to recognizing design before writing any code at all.

The same vocabulary works in the other direction too, reading what the model hands back. A generated class named `ShippingStrategyFactory` that returns one of three hardcoded concrete types based on an enum, with no injected dependency and no open extension point, has not built a Factory Method in the sense the catalogue describes. It has built a switch statement wearing a factory's name. Naming the forces lets you ask the follow-up question immediately: where is the decision this factory is supposed to encapsulate, and who makes it. If the answer is "the enum value, chosen at the call site," the factory adds a layer of indirection around a decision that was already made before the factory was called, and the simpler form, a direct constructor call per branch, is the one the forces support.

Naming patterns in a prompt also works for ruling a pattern out. "This configuration value is read in one place, set once at startup, and nothing in this codebase needs a second implementation of how it is sourced, so do not wrap it behind an interface" is a legitimate and useful instruction, and it short-circuits the single most common AI failure mode this chapter covers next: reaching for an abstraction that nothing in the problem calls for.

## The failure modes of an unchecked collaborator

Three failure modes show up often enough in AI-generated C++ to name individually, and all three share a root cause: a model optimizing for "a plausible-looking answer to this prompt" rather than for "the smallest correct answer to the actual problem."

**Over-patterning a small problem.** Ask for a function that reads a single boolean flag from a JSON config and returns it, and a sufficiently eager model will sometimes hand back a `ConfigProvider` interface, a `JsonConfigProvider` implementation, a `ConfigProviderFactory` that returns the one implementation, and a `ConfigSingleton` holding the instance everything else resolves through. Every piece of that answer is individually defensible: interfaces decouple consumers from implementations, factories decouple construction from use, a singleton guarantees one shared instance. None of those justifications is a force present in reading one boolean out of one file. This is the same reflex this book's anti-patterns material calls patternitis, a pattern applied because its shape is familiar rather than because the code in front of you calls for it, and an LLM trained on a corpus containing a great deal of over-engineered example code reproduces that reflex reliably whenever a prompt leaves room for it. The fix is the same forces test applied everywhere else in this book: for every type the model introduces, ask what force justifies it existing, and delete the ones with no answer.

**Hallucinated APIs.** A model will, with complete confidence and syntactically valid-looking code, call a standard library function that does not exist, pass arguments in an order the real function never accepted, or invoke a member function on a type from a library version newer or older than the one pinned in the project. This is not a rare edge case; it is a predictable consequence of how these models generate text, producing the token sequence that looks most like correct code for the surrounding context, with no compiler in the loop to reject what it wrote until a human or a build step catches it. A generated call into `std::ranges` with a signature that belongs to a different overload, or a constructor for a Conan-packaged library that matches an older major version than the one `conanfile.py` actually pins, reads as entirely plausible to someone skimming it and fails the instant it is compiled, or worse, compiles against a similarly named but behaviorally different overload and fails only at runtime. The practical defense is unglamorous: compile it, and do not treat a model's description of an API's behavior as a substitute for the documentation or the header file.

**Non-idiomatic or outdated-standard C++.** Training data spans decades of C++ written against standards from C++98 forward, and a model has no inherent preference for the code this project targets. The result shows up as `new` and `delete` where RAII and a smart pointer belong, raw pointers returned from factory functions where `std::unique_ptr` or `std::shared_ptr` communicates ownership in the type itself, manually written loops where a range-based `for` or a `std::ranges` algorithm says the same thing more clearly, and pre-C++11 idioms, a `typedef` instead of `using`, a function returning a status code through an output parameter instead of `std::optional` or `std::expected`, that compile cleanly and work correctly while contradicting every idiom this book has spent its chapters establishing. None of this code is wrong in the sense of producing an incorrect answer. It is wrong in the sense that it does not look like the rest of the codebase it is joining, and a reviewer who accepts it on the strength of "it compiles and the tests pass" has let the project's idiomatic baseline erode one generated function at a time.

> **AI sidebar.** Ask a model to "fix the bug where line 40 doesn't handle the error case" and it will frequently do exactly that, and nothing more, inserting a `try`/`catch` or an `if` check scoped as narrowly as the sentence that asked for it. That narrow scoping has a consequence: a model answers the question it was asked, not the question you meant to ask, and it has no instinct for a related bug three lines away unless you point at it. The inverse failure is just as real: ask it to "make this more robust" with no scope at all, and it will frequently touch far more than the one function you had in mind, restructuring working code nearby on the theory that robustness is a property of the whole file. Scope the request as precisely as you would scope a code review comment, naming the function, the input, and the specific failure you want handled, and treat anything the response touches outside that scope as a change that needs its own look before it merges.

## The verification discipline that makes AI output safe

Nothing in the three failure modes above is detectable by reading the code and nodding. An over-patterned config reader compiles. A hallucinated API sometimes compiles against the wrong overload and runs without crashing on the one input a casual smoke test happens to exercise. Non-idiomatic C++ compiles by definition, since idiom is not something the compiler checks. The only thing that catches all three reliably, applied the same way regardless of who or what wrote the code, is the same discipline this book has applied to every pattern in its catalogue: a behavior test that proves the code does what it claims, not merely that the binary exits zero.

Two gates sit between an AI-drafted pattern and a merge, and both are required, not either-or. The first is a behavior test suite, written against the specification the code is supposed to satisfy rather than against whatever the generated implementation happens to do. A test suite written by asking the same model "write tests for the function you just wrote" inherits the model's own blind spots; if it misunderstood the tier boundary as exclusive on the low end, a self-generated test suite will usually encode that same misunderstanding as the expected answer; passing tests of that kind without a human writing or reviewing the test table. The second gate is an idiomatic-code review pass, checking the draft against the project's actual conventions, RAII over manual lifetime management, `std::expected` or `std::optional` over output parameters, concepts over unconstrained templates where C++20 makes a concept available, and fixing what the compiler will never flag because idiom is not a compiler's job.

Both gates exist because they catch different classes of defect. A behavior test proves the code is correct against a specification a human wrote down and understood. An idiomatic review proves the code fits the codebase it is joining, so that the next engineer, human or AI, reading this file inherits a consistent baseline rather than a patchwork of whichever decade's C++ habits happened to generate each function. A pattern that passes its behavior tests but reads like it was written against an older standard than the rest of the file has still failed half of what this discipline checks for.

AI tooling is useful on the other side of this discipline too, not only as the thing being checked but as a tool for building the checks themselves. Characterization tests, the technique of locking down an existing system's actual behavior before changing anything near it, are exactly the kind of mechanical, exhaustive work a model handles well: given an existing function and a request to "generate a test for every branch this function currently takes, asserting whatever it currently returns, without changing anything," a model can produce a wide net of coverage faster than a human typing the same cases by hand. The same applies to finding seams: given a function wrapped around a hard dependency, asking a model "where could this function's dependency on this concrete type be broken without changing its behavior" produces candidate seam locations worth evaluating. What a model should never decide unsupervised is whether a given seam is safe to cut in a system with side effects or shared state the model cannot see the full extent of; that judgment call, like the pattern-selection judgment call this whole book trains, belongs to the engineer who understands the system's actual blast radius.

## Catching the bug in code: a usage-tier Strategy

The example behind this chapter, in `examples/32-ai-and-llm-tools`, builds exactly the scenario the verification discipline exists to catch. The prompt an engineer might give an AI pairing partner here is specific and forces-based: "a metered API gateway classifies a customer's monthly request count into one of three billing tiers, Starter, Growth, or Scale, and the classification rule varies by nothing except that count, so write it as a Strategy with the usual shape, an abstract interface and a concrete classifier behind it." `UsageTierStrategy` (`src/usage_tier_strategy.hpp`) is the resulting interface, one virtual method, `Classify(long monthly_requests)`, returning a `UsageTier`.

`CorrectUsageTierStrategy` (`src/correct_usage_tier_strategy.hpp`) implements the pricing spec's actual boundaries: Starter below 10,000 requests a month, Growth from 10,000 up to 99,999, Scale from 100,000 up. Both lower bounds are inclusive, so a customer's exact 10,000th request of the month has crossed into Growth, not stayed in Starter.

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

`PlausibleWrongUsageTierStrategy` (`src/plausible_wrong_usage_tier_strategy.hpp`) is the shape an AI pairing partner handed back for the identical prompt: the same interface, the same three tiers, the same early-return structure a reviewer skimming for "does this look like a Strategy" would wave through. It carries one known bug, a single comparison operator, `<=` instead of `<`, on the Starter and Growth boundary:

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

Reading the two implementations side by side, the bug is easy to miss. Both functions have the identical structure, the identical number of branches, and the identical second comparison; only the operator on the first branch differs by one character, and `<=` reads as an entirely reasonable choice for an inclusive-sounding tier name if you are not holding the pricing spec's exact wording in your head while you review it. This is precisely the shape this chapter's failure modes describe: syntactically clean, structurally idiomatic, and wrong at exactly one input a read-only review is unlikely to notice.

`tests/usage_tier_classification_test.cpp` is the gate. A single shared function, `ExpectSpecCompliantClassification`, asserts the full boundary table, every tier's low end, middle, and high end, against whichever strategy it is given:

```cpp
void ExpectSpecCompliantClassification(const UsageTierStrategy& strategy) {
    EXPECT_EQ(strategy.Classify(0), UsageTier::Starter);
    EXPECT_EQ(strategy.Classify(9'999), UsageTier::Starter);
    EXPECT_EQ(strategy.Classify(10'000), UsageTier::Growth);
    EXPECT_EQ(strategy.Classify(99'999), UsageTier::Growth);
    EXPECT_EQ(strategy.Classify(100'000), UsageTier::Scale);
}
```

Run against `CorrectUsageTierStrategy`, every assertion in that table passes, and that test is what a continuous integration pipeline actually gates a merge on. Run the identical table against `PlausibleWrongUsageTierStrategy`, and the 10,000-request case alone disagrees with the spec; every other case still matches, which is exactly what makes a one-operator bug this easy to miss on a casual read and this reliable to catch with a table that bothers to test every boundary rather than one value comfortably inside each tier. The test file proves this directly, wrapping the shared table in GoogleTest's own failure-inspection macro so the suite demonstrates the catch without the overall build going red:

```cpp
EXPECT_NONFATAL_FAILURE(
    ExpectSpecCompliantClassification(plausible_wrong_strategy),
    "10'000");
```

`tests/billing_test.cpp` turns the same bug into a dollar figure, because a billing-tier misclassification is not an abstract correctness issue, it is a wrong invoice. At exactly 10,000 requests, the correct classification bills a flat $99 Growth rate; the plausible-wrong classification keeps the customer on Starter's $0.02-per-request metering, producing a $200 bill for the same month, a $101 overcharge traceable to one comparison operator. That number is the concrete argument for why a pattern this small still goes through the verification gate: nothing about a Strategy with three branches looks risky enough to skip testing, and the risk was never in the pattern's complexity. It was in the boundary a plausible-looking draft got wrong.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain from the prerequisites chapter; nothing in this loop uses a container. From the repository root, the superbuild picks up `examples/32-ai-and-llm-tools` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Nothing in this chapter's code depends on a C++23-only feature, so both presets discover the identical set of gtest cases via `gtest_discover_tests`. To see the correct and plausible-wrong strategies classify the same requests side by side, and to see the billing gap the boundary bug produces, run the example's own demo script from its own directory:

```sh
examples/32-ai-and-llm-tools/demo.sh cpp23
```

It prints both strategies' classification for a sample of request counts spanning every tier boundary, then isolates the exact 10,000-request case where they disagree and the dollar amount that disagreement costs. Running `examples/32-ai-and-llm-tools/demo.sh cpp20` prints identical output, since nothing here is standard-dependent; a codetabs block contrasting a C++23-only expression for `Classify`'s return path against a C++20 equivalent would compile under both presets with no behavioral difference worth showing, which is itself a small instance of this chapter's own discipline: add the contrast only where a real difference exists, not because the book's format has room for one.

## When not to use it

An LLM pairing partner is worth reaching for on a task with a clear specification and a cheap way to check the answer: a function whose contract is well understood, a test suite that can be written or already exists, a refactor whose behavior-preservation can be proven mechanically. Reaching for one costs more than it returns in a few recurring situations.

When the specification itself is the hard part, deciding what the system should do rather than how to make it do that, an LLM's fluent-sounding answer to an underspecified question is a liability, not a shortcut: it will produce something plausible for almost any prompt, and a plausible-sounding wrong answer to an unclear requirement is more dangerous than an obviously incomplete one, because it looks finished. When a change touches shared mutable state, concurrency, or an invariant that spans multiple files the model was not shown, the model's view of "this looks safe" is bounded by what fits in its context window, not by what the system actually guarantees; a seam that looks safe to cut in isolation can break an invariant a different file depends on, and only an engineer who holds the whole system's shape has the standing to make that call. When the actual bottleneck is a design decision, which pattern the forces call for, whether an abstraction is premature, whether a class has outgrown its single responsibility, delegating the decision to a model does not speed up the work; it just moves the judgment call to someone, or something, less equipped to make it, and the engineer ends up re-deriving the same forces analysis afterward to check the model's choice anyway.

The judgment calls that stay with the engineer, regardless of how capable the tooling gets, are the same ones this book has trained from its first chapter: naming the forces present in the actual problem, deciding whether a pattern's cost is worth paying for the benefit it buys, and accepting responsibility for the answer once it ships. An LLM can draft any of this book's patterns in seconds and can draft the test suite that checks its own draft just as fast. It cannot tell you whether the problem in front of you needed a pattern at all, and it has no stake in being right. That stake is the one thing this chapter cannot delegate.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480), Addison-Wesley: the shared pattern vocabulary this chapter treats as the actual interface between an engineer and an AI pairing partner, and the same catalogue a model's over-patterning reflex misapplies when no force in the code calls for it.
- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445), Prentice Hall: the Single Responsibility and Dependency Inversion reasoning behind the idiomatic-review gate this chapter applies to AI-drafted code, the same forces test this book has used on every pattern in its catalogue.
- Michael Feathers, [*Working Effectively with Legacy Code*](https://www.oreilly.com/library/view/working-effectively-with/0131177052/), Prentice Hall: the characterization-test technique and seam-finding discipline this chapter hands to an AI pairing partner for the mechanical half of legacy work, while keeping the decision of which seam is safe to cut with the engineer.
- Hyuk Myeong, *Googletest in Practice*, self-published: the GoogleTest idioms behind the behavior-test gate this chapter runs against both the correct and the plausible-wrong usage-tier Strategy, read alongside the official [GoogleTest documentation](https://google.github.io/googletest/).

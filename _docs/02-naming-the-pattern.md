---
title: "Naming the pattern: shared vocabulary for teams and AI assistants"
order: 2
part: "recognizing-design"
description: "How a pattern's name turns a paragraph of trade-off reasoning into a word a reviewer can act on, and why that same compression is now the sharpest tool you have for prompting and reviewing an AI pairing partner."
duration: "20 min"
---

## Learning objectives

By the end of this chapter you can:

- Say what a pattern name compresses: a set of forces plus a chosen structure, not a block of code.
- Use a pattern name in review so the discussion is about the choice, not the syntax.
- Write a prompt that gives an AI assistant the forces, not just the pattern's name.
- Spot a needlessly heavy pattern in generated C++, such as a virtual interface where a `std::function`, a template parameter or a concept would do.

## Why this matters

A reviewer writes: "this needs to be a Strategy, not a switch, because partner tiers are now owned by separate teams." Everyone on the thread knows what the word stands for: a rule that varies, variants owned by more than one team, and a `switch` that would make every team edit one shared function. Nobody re-derives the trade-off.

That compression breaks when the word arrives without the forces. A model trained on a lot of code has seen pattern names used loosely, so the same word that lets two engineers skip a paragraph can let an assistant skip the reasoning that justified the word.

{% include excalidraw.html name="02-shared-vocabulary" caption="Figure 2.1: A pattern name sits between three readers, a teammate, a future-you, and an AI assistant, compressing a cluster of forces and a chosen structure into one word all three can act on the same way." %}

## A name is a compression, not a label

"Strategy" is shorthand for a paragraph: something in this code varies, the caller shouldn't know how, and several variants must coexist so that adding one doesn't change the caller. Say "abstract base class with virtual dispatch" instead and you've described a shape without saying why it exists.

The diff already shows the shape. What it doesn't show is whether the forces that justify the shape are present in this code, for this team, today. Check a name by unpacking it:

| Asked "why a Strategy and not a function with a branch?" | The name is |
| --- | --- |
| The answer names which requirements compete and which one wins | still attached to its forces |
| The answer is "that's what you do when a value varies" | decoration; the forces were never checked |

## Reviewing at the level of the choice

A pull request adds a connection pool to a billing service's database layer. Read line by line, the review checks the mutex, the destructor and the tests. With shared vocabulary, it asks the earlier question: does this service need what a pool guarantees, that no two callers hold the same live connection, or is one shared connection behind a mutex enough for its traffic?

The pull request description decides which review you get:

| Description | Invites |
| --- | --- |
| "Implements Strategy for fee calculation." | a check that Strategy is implemented correctly |
| "Fee calculation now varies per partner tier, owned by separate teams. Strategy over a `switch`, because the `switch` would make every team edit one shared function." | a check that the forces are real and the plain alternative was weighed |

The same applies to a design meeting. If construction varies by concrete type and is too involved to inline at every call site, a Factory is worth it. If `std::make_shared<ConcreteType>()` already says it, a Factory is a layer with nothing under it.

## The same word, given to an AI assistant

A prompt with the forces in it gets the same choice a careful engineer would make:

> Add a Strategy for computing shipping cost, switchable at run time between flat-rate and distance-based pricing. A third scheme must not change the caller.

The prompt says what varies, that the choice happens at run time, and what must hold. With those, an abstract interface behind a `std::unique_ptr` is the right answer.

A prompt without them leaves the choice to whatever shape is most common near similar code:

> Make this flexible, use a design pattern.

That's the opening case of Chapter 1, the engineer who turns every `if` into a Strategy, produced in seconds. The fix is the same for a person and a model: state the forces before the name.

Generated code also gets the same two review questions as hand-written code. A pull request that says "implements Observer for cache invalidation" has to answer which forces it resolves and whether a plainer alternative was considered.

## Spotting a heavy pattern in generated C++

Ask for "a pluggable discount calculator" for a retail checkout, and a common reply has:

- an abstract `DiscountCalculator` with a pure virtual `apply`;
- two or three concrete subclasses;
- a factory function that picks one by an enum;
- a `std::unique_ptr<DiscountCalculator>` passed through the checkout path.

Each piece is correct C++. If the real requirement is "one of three fixed rules, chosen when the program starts, with no plugins," the hierarchy solves a flexibility problem the code doesn't have, at the cost of a virtual call on the checkout path, three extra files and a factory to maintain. Match the expression to when the choice is made:

| Discount rule is chosen | Write |
| --- | --- |
| at run time, from configuration or a plugin that can change | the virtual interface |
| once, from a configuration value read at startup | a `std::function<Money(Money)>` assigned during setup |
| when the program is built | a template parameter |
| by shape only, with no base class needed | a C++20 concept constraining the rule type |

The tell is usually proportion: one production implementation, plus a second that exists only to prove the interface is pluggable.

## Why recognition matters more now

When writing a class hierarchy took an afternoon, the typing gave you time to notice it solved nothing. A prompt for "a flexible, extensible discount system" now returns the hierarchy, the factory and a README in less time than one class used to take, and that friction is gone.

So ask the question explicitly, before sending the prompt and before accepting the diff: what are the forces, and does this pattern or the plain alternative answer them? An assistant can produce every step of Chapter 1's five steps on request except the first, recognizing the forces in your code. That one stays with you.

Every pattern chapter adds a name, its plain alternative and a "When not to use it" section. Part 2 turns to the SOLID principles, which explain why some of these forces recur so often.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the catalogue whose names this chapter treats as shared vocabulary, and the source worth reading in full once a pattern's name alone stops being enough to settle a review.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); every citation in this book is a pointer to the source, not a substitute for reading it.

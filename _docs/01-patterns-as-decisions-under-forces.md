---
title: "Patterns as decisions under forces"
order: 1
part: "recognizing-design"
description: "Why a design pattern is a named response to a recurring set of trade-offs rather than a recipe to apply from memory, and the five steps every pattern chapter follows."
duration: "20 min"
---

## Learning objectives

By the end of this chapter you can:

- Say why a pattern is a name for a recurring trade-off, not a template to copy.
- Name the competing requirements (the forces) behind a design choice in a piece of code.
- Walk the five steps every pattern chapter follows: recognize the forces, name a pattern and a plain alternative, choose the C++ expression, test the behavior, and know when not to use it.
- Pick between a pattern and its plain alternative for a concrete case.

## Why this matters

An engineer fresh from the Gang of Four book meets a function with an `if` and turns it into Strategy: an abstract base class, three one-line overrides and a factory to pick among them. It compiles and it runs. It's also worse than the three-line branch it replaced, because the name came before the question of what the code needed.

A pattern is a name for a recurring set of forces: requirements that pull against each other so that no single piece of code satisfies all of them at no cost. Find the forces first and reach for the name second.

{% include excalidraw.html name="01-forces-and-decisions" caption="Figure 1.1: The arc this book trains for every pattern: recognize the forces, name a candidate pattern and a candidate non-pattern, choose the idiomatic C++ expression, test the behavior, and know when not to use it." %}

## A pattern is a name, not a recipe

The Gang of Four define a pattern as a description of a problem that recurs, with the core of its solution, stated generally enough to apply again without writing the same code twice. A recipe says what to type. A pattern says what problem you're looking at and leaves the code open.

Two engineers who both see a Strategy problem can write different C++ for it:

| Situation | Expression | Why |
| --- | --- | --- |
| The pricing rule is picked from configuration at run time | abstract base class, held by `std::unique_ptr` | the choice happens after the program starts |
| The rule is fixed when the program is built, on a hot path | template parameter | no virtual call, resolved at compile time |
| The rule is a small function chosen once at startup | `std::function` holding a lambda | no class hierarchy at all |

All three are Strategy. Remember the forces and you'll recognize the pattern in code that looks nothing like the listing that taught it to you. Remember one code shape and you'll misapply it the first time the forces differ.

## Forces that recur

A force is a requirement that competes with another one in the same code. These pairs come up in nearly every pattern chapter:

| This | pulls against | Example |
| --- | --- | --- |
| Flexibility | simplicity | a swappable backend has more moving parts than a fixed one |
| Compile-time resolution | build time and readability | templates remove the indirect call but lengthen builds and debugger traces |
| Run-time dispatch | call cost | virtual calls keep builds fast and traces legible, at a vtable lookup per call |
| Testability | directness | an injected interface takes a fake; a direct call is one line shorter and one test harder |
| Extensibility | YAGNI | a seam for a requirement nobody asked for is paid for now and may never be used |

None of them has a universal winner. A telemetry pipeline that picks a file or network exporter from an environment variable at startup has a real flexibility requirement. A billing engine's tax calculation, fixed by the jurisdictions one deployment serves, usually doesn't, and an abstraction built for "swap the tax rules" pays only in the deployment that needs it.

## The five steps

Every pattern chapter runs the same five steps in the same order.

**1. Recognize the forces.** Ask what varies, what stays fixed, and what's expensive to get wrong. In a billing engine that charges a different fee per account tier, the fee calculation varies and the rest of checkout stays fixed. In a connection pool, handing the same connection to two threads is expensive to get wrong, so the design has to prevent it by construction. Describe the tension before you name anything.

**2. Name a pattern and a plain alternative.** For the fee calculation the pattern is Strategy, one implementation per tier. The plain alternative is one function with a `switch` over the tier. Name both every time, then let the forces choose:

| Fee logic | Choose |
| --- | --- |
| Three tiers, changes twice a year, one team owns it | the `switch` |
| Eleven tiers, a new partner tier every sprint, separate teams per tier | Strategy: each tier in its own file, added without touching shared code |

**3. Choose the C++ expression.** The name constrains the shape, not the code. Modern C++ offers choices the 1994 catalogue didn't have, as the Strategy table above shows, and picking among them is part of the design. Part 4 and Part 8 compare them with compiling code.

**4. Test it.** A pattern claims a behavior. A Strategy claims that swapping the implementation changes the outcome without changing the caller; a connection pool claims that no two callers hold the same live connection. A binary that compiles and exits zero proves neither. Every example carries a GoogleTest suite that checks the claimed behavior, so a claim you can't write as a failing assertion isn't pinned down yet.

**5. Know when not to use it.** A Strategy with one implementation and no second one coming is an abstraction paying rent for a future that hasn't arrived. A Factory around a constructor that does nothing more than construct adds a layer with nothing under it. Every pattern chapter ends with this section, because it's the other half of the same choice.

## In review and with an AI assistant

A pattern name turns a paragraph of trade-offs into one review comment: "this needs to be a Strategy, not a switch, because partner tiers are now owned by separate teams." That works only while everyone means the same forces by the word.

The same holds for an AI pairing partner. A model has read a lot of code that uses pattern names loosely, so a suggestion carrying a pattern's name gets the same two questions: what are the forces here, and would the plain alternative answer them? Chapter 2 covers prompting and reviewing with pattern names.

## Where the steps lead

| Part | Adds |
| --- | --- |
| Part 2 | SOLID, which states several of these forces as principles; "testability against directness" is close kin to Dependency Inversion |
| Part 3 | the C++ substrate every pattern depends on: RAII, value semantics, and GoogleTest |
| Part 4 onward | one pattern per chapter, run through the five steps against a real-domain example such as a reservation system or a telemetry pipeline |

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original definition of a pattern as a recurring problem and the core of its solution, and the catalogue this book's chapters track pattern by pattern.
- Robert C. Martin, [*Agile Software Development: Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the SOLID principles Part 2 builds on directly, and an early, explicit treatment of patterns chosen by the forces in front of you rather than applied from memory.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); every citation in this book is a pointer to the source, not a substitute for reading it.

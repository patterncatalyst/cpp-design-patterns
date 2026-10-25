---
title: "Pattern-to-force quick reference"
order: 37
layout: tutorial
sectionid: docs
part: appendices
appendix: "E"
description: "A completeness ledger for the whole book: every GoF pattern, SOLID principle, and C++ idiom mapped to the force that justifies reaching for it, each linked to the chapter that builds it. Scan it to find the tool for a force in front of you, or to confirm the catalogue is complete."
---

## How to read this ledger

A pattern is a response to a force, a pressure in the design that the plain,
direct solution handles badly. This book teaches patterns in that order:
recognize the force, then name the pattern that answers it. This appendix inverts
the book so you can scan from the force back to the tool.

Each row names a pattern, principle, or idiom, states the force that justifies
reaching for it, and links the chapter that builds it against a runnable example.
The right-hand column is the test: if no force in that column is present in the
code in front of you, applying the pattern is not a neutral choice, it is the
over-engineering the anti-patterns chapter warns about. Match the mechanism to
the force actually present, not to the force a textbook example implies.

The ledger covers all twenty-three Gang of Four patterns, the five SOLID
principles, and the C++ idioms the book treats as first-class patterns in their
own right.

## Principles (SOLID)

The principles are not patterns; they are the forces-and-tradeoffs vocabulary the
patterns are judged against.

| Principle | Reach for it when | Chapter |
|-----------|-------------------|---------|
| Single Responsibility | A class has more than one reason to change, so one concern's edit risks another | [SOLID I]({{ '/docs/03-solid-first-principles/' | relative_url }}) |
| Open/Closed | New behavior should arrive as new code, not edits to working code | [SOLID I]({{ '/docs/03-solid-first-principles/' | relative_url }}) |
| Liskov Substitution | A subtype must be usable anywhere its base is, without surprising a caller | [SOLID I]({{ '/docs/03-solid-first-principles/' | relative_url }}) |
| Interface Segregation | A client is forced to depend on methods it never calls | [SOLID II]({{ '/docs/04-solid-dependency-inversion/' | relative_url }}) |
| Dependency Inversion | High-level policy should not depend on low-level detail; both depend on an abstraction | [SOLID II]({{ '/docs/04-solid-dependency-inversion/' | relative_url }}) |

## Foundational C++ idioms

The substrate the rest of the catalogue is built on.

| Idiom | Reach for it when | Chapter |
|-------|-------------------|---------|
| RAII and deterministic lifetimes | A resource must be released exactly once, on every path out, including exceptions | [RAII]({{ '/docs/05-raii-and-deterministic-lifetimes/' | relative_url }}) |
| Value semantics and ownership | Who owns an object and who merely observes it must be clear in the type, not the comments | [Value semantics]({{ '/docs/06-value-semantics-and-ownership/' | relative_url }}) |

## Creational patterns

| Pattern | Reach for it when | Chapter |
|---------|-------------------|---------|
| Factory Method | Which concrete type to build is a decision resolved at runtime, not at the call site | [Factory Method and Abstract Factory]({{ '/docs/12-factory-method-and-abstract-factory/' | relative_url }}) |
| Abstract Factory | A whole family of related objects must be built in a consistent variant together | [Factory Method and Abstract Factory]({{ '/docs/12-factory-method-and-abstract-factory/' | relative_url }}) |
| Builder | An object has many optional parts and construction should read as a sequence of choices | [Builder and Prototype]({{ '/docs/13-builder-and-prototype/' | relative_url }}) |
| Prototype | A new object is best made by copying a configured exemplar rather than rebuilding it | [Builder and Prototype]({{ '/docs/13-builder-and-prototype/' | relative_url }}) |
| Singleton | Exactly one instance must exist, it is effectively immutable, and nothing needs to substitute another | [Singleton and the DI alternative]({{ '/docs/14-singleton-and-the-di-alternative/' | relative_url }}) |

## Structural patterns

| Pattern | Reach for it when | Chapter |
|---------|-------------------|---------|
| Adapter | An existing type has the right behavior behind the wrong interface | [Adapter, Facade, Bridge]({{ '/docs/15-adapter-facade-bridge/' | relative_url }}) |
| Facade | A subsystem's many moving parts should present one small, intentional surface | [Adapter, Facade, Bridge]({{ '/docs/15-adapter-facade-bridge/' | relative_url }}) |
| Bridge | An abstraction and its implementation must vary independently | [Adapter, Facade, Bridge]({{ '/docs/15-adapter-facade-bridge/' | relative_url }}) |
| PImpl | A class should hide its implementation to cut compile-time coupling across a header | [Adapter, Facade, Bridge]({{ '/docs/15-adapter-facade-bridge/' | relative_url }}) |
| Decorator | Behavior should be added to one object at runtime without subclassing every combination | [Decorator and Proxy]({{ '/docs/16-decorator-and-proxy/' | relative_url }}) |
| Proxy | Access to an object needs a stand-in for lazy creation, caching, or control | [Decorator and Proxy]({{ '/docs/16-decorator-and-proxy/' | relative_url }}) |
| Composite | Clients should treat a single object and a tree of objects the same way | [Composite, Flyweight, Null Object]({{ '/docs/17-composite-flyweight-null-object/' | relative_url }}) |
| Flyweight | Many objects share most of their state and the duplication is too expensive to keep | [Composite, Flyweight, Null Object]({{ '/docs/17-composite-flyweight-null-object/' | relative_url }}) |
| Null Object | A do-nothing case should remove a null check from every call site | [Composite, Flyweight, Null Object]({{ '/docs/17-composite-flyweight-null-object/' | relative_url }}) |

## Behavioral patterns

| Pattern | Reach for it when | Chapter |
|---------|-------------------|---------|
| Strategy | An algorithm chosen by a caller must be swappable without touching the caller | [Strategy]({{ '/docs/08-strategy/' | relative_url }}) |
| Template Method (and NVI) | A fixed sequence has steps that vary, and the sequence itself must not | [Template Method and NVI]({{ '/docs/09-template-method-and-nvi/' | relative_url }}) |
| State | Behavior depends on a current status, and illegal transitions must be refused | [State and finite state machines]({{ '/docs/10-state-and-finite-state-machines/' | relative_url }}) |
| Command | A request must become an object so it can be queued, logged, or undone | [Command, undo, and queues]({{ '/docs/11-command-undo-and-queues/' | relative_url }}) |
| Observer | Subscribers must react to a change without the subject knowing who they are | [Observer and Mediator]({{ '/docs/18-observer-and-mediator/' | relative_url }}) |
| Mediator | Many objects' direct references to each other have become a tangle | [Observer and Mediator]({{ '/docs/18-observer-and-mediator/' | relative_url }}) |
| Chain of Responsibility | A request should pass along a line of handlers until one takes it | [Chain of Responsibility and Iterator]({{ '/docs/19-chain-of-responsibility-and-iterator/' | relative_url }}) |
| Iterator | A collection's elements must be traversed without exposing its structure | [Chain of Responsibility and Iterator]({{ '/docs/19-chain-of-responsibility-and-iterator/' | relative_url }}) |
| Visitor | A new operation must be added across a fixed set of types without editing each | [Visitor, Memento, Interpreter]({{ '/docs/20-visitor-memento-interpreter/' | relative_url }}) |
| Memento | An object's state must be captured and restored without exposing its internals | [Visitor, Memento, Interpreter]({{ '/docs/20-visitor-memento-interpreter/' | relative_url }}) |
| Interpreter | A small, recurring grammar is best expressed as types that evaluate themselves | [Visitor, Memento, Interpreter]({{ '/docs/20-visitor-memento-interpreter/' | relative_url }}) |

## Idiomatic C++ expressions

Patterns that are specific to C++ rather than general object-oriented design.

| Idiom | Reach for it when | Chapter |
|-------|-------------------|---------|
| CRTP and policy-based design | Polymorphism is needed with no runtime cost and the type is known at compile time | [CRTP and policy-based design]({{ '/docs/21-crtp-and-policy-based-design/' | relative_url }}) |
| Type erasure | Unrelated types must share one value-semantic interface with no common base class | [Type erasure and PImpl]({{ '/docs/22-type-erasure-and-pimpl/' | relative_url }}) |
| Expression templates | A chain of operations should evaluate in one pass with no intermediate objects | [Expression templates and generic idioms]({{ '/docs/23-expression-templates-and-generic-idioms/' | relative_url }}) |
| Self-registering registry | Implementations should register themselves with no central list to edit | [Expression templates and generic idioms]({{ '/docs/23-expression-templates-and-generic-idioms/' | relative_url }}) |
| Templated serialization | One field description should drive both reading and writing a type | [Expression templates and generic idioms]({{ '/docs/23-expression-templates-and-generic-idioms/' | relative_url }}) |

## Patterns in the wild

| Pattern | Reach for it when | Chapter |
|---------|-------------------|---------|
| Resource pool | An expensive-to-create resource must be reused under a bound | [Resource pools]({{ '/docs/24-resource-pools/' | relative_url }}) |
| Monitor object | An object's methods must be serialized against concurrent access | [Resource pools]({{ '/docs/24-resource-pools/' | relative_url }}) |
| Active object | A method call must be decoupled from the thread that runs it | [Resource pools]({{ '/docs/24-resource-pools/' | relative_url }}) |
| Dispatch table or map | A key selects one short handler and a full class hierarchy would be overkill | [Shortcut patterns: dispatch tables and maps]({{ '/docs/25-dispatch-tables/' | relative_url }}) |
| Recognizing an anti-pattern | A pattern is present with no force that calls for it | [Anti-patterns and pattern misuse]({{ '/docs/26-anti-patterns/' | relative_url }}) |

## Changing inherited code

| Technique | Reach for it when | Chapter |
|-----------|-------------------|---------|
| Characterization test | Inherited code must be pinned before it is safe to change | [Seams and the test safety net]({{ '/docs/27-seams/' | relative_url }}) |
| Seam (object, link, preprocessor) | Behavior must be altered without editing the logic in place | [Seams and the test safety net]({{ '/docs/27-seams/' | relative_url }}) |
| Dependency-breaking techniques | A hard-wired dependency blocks a class from being tested | [The 24 dependency-breaking techniques]({{ '/docs/28-dependency-breaking/' | relative_url }}) |
| Fowler-then-Feathers workflow | Inherited code needs both a safety net and a design cleanup | [Fowler and Feathers]({{ '/docs/29-fowler-and-feathers/' | relative_url }}) |
| Strangler fig | A legacy component must be replaced incrementally with no big-bang cutover | [Refactor or rewrite]({{ '/docs/30-refactor-or-rewrite/' | relative_url }}) |

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the canonical catalogue this ledger indexes, where each pattern's intent, structure, and consequences are set out in full.
- Robert C. Martin, [*Agile Software Development, Principles, Patterns, and Practices*](https://www.pearson.com/store/p/agile-software-development-principles-patterns-and-practices/P200000003363/9780135974445) (Prentice Hall): the SOLID principles the ledger's principle rows condense.

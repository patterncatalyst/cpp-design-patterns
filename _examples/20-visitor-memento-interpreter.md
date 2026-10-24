---
layout: example
order: 20
title: "Visitor, Memento, and Interpreter"
description: A merchant shipping-rule AST walked by classic and variant visitors, interpreted for matches, and snapshotted by a memento.
github_path: examples/20-visitor-memento-interpreter
card_eyebrow: "Chapter 20 · Visitor / Memento / Interpreter"
card_title: "Shipping rules: visit, interpret, snapshot"
card_blurb: "The same six-node rule AST walked two ways (classic double-dispatch and std::variant/std::visit), interpreted to a match result, with a rule-set editor whose memento restores whole state."
---

A merchant-defined shipping-filter rule language that ties three patterns to one
AST (`WeightAbove`, `ZoneIs`, `DeclaredValueAbove`, `And`, `Or`, `Not`):

- **Visitor, two ways.** The classic GoF double-dispatch visitor (`RuleNode`
  hierarchy + `RuleVisitor` interface, `MatchEvaluator` and `RuleDescriber` add
  operations with no node changes) and the modern form (the same kinds as a
  recursive `std::variant` dispatched by `std::visit` + an `Overload` set). Tests
  assert both agree.
- **Interpreter.** The same tree-walking `evaluate()` interprets the AST as a
  grammar; nesting and precedence are proven by two trees of identical leaves
  that evaluate differently.
- **Memento.** `RuleSetEditor` is the originator; its opaque `Snapshot` captures
  whole state and `restore()` rolls it back in one call (contrasted with ch11's
  per-operation command undo, and deep-cloning rules via ch13's Prototype idea).

A C++23 `try_evaluate_rule()` returning `std::expected` is guarded for C++20.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 20/20 tests pass
./examples/20-visitor-memento-interpreter/demo.sh cpp23
```

The demo matches and describes a rule two ways, interprets nested rules, and
snapshots then restores an editor. Chapter 20 covers when each pattern fits and
closes the 23 GoF patterns. Builds standalone and under the `cpp20` preset
(18 tests).

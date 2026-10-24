# 20 - Visitor, Memento, and Interpreter: shipping-filter rules and a draft snapshot

A checkout platform lets merchants define their own shipping filters, a
rule tree of predicates (`WeightAbove`, `ZoneIs`, `DeclaredValueAbove`)
combined with `And`, `Or`, and `Not`. This example builds that tree two
idiomatic C++ ways to teach Visitor, walks it to evaluate a small grammar
to teach Interpreter, and snapshots a merchant's whole in-progress rule
set to teach Memento.

## The domain

- `Shipment`: `weight_kg`, a `DestinationZone`, and `declared_value_cents`,
  the same shape this book has used since Chapter 8.
- A rule is one of six node kinds: three leaf predicates and `And`/`Or`/
  `Not` to combine them.
- Money is tracked as integer `Cents`, never floating point, so a
  threshold comparison is exact and reproducible in a test assertion.

## Visitor: two idiomatic expressions of the same six node kinds

1. **Classic (`src/classic_visitor.hpp/.cpp`)**: a `RuleNode` class
   hierarchy and a `RuleVisitor` interface with one pure virtual `visit()`
   per node kind, the GoF double-dispatch shape. `MatchEvaluator` answers
   "does this shipment match"; `RuleDescriber` renders a rule as a
   readable string. Both ship with zero changes to `RuleNode` or any of
   its six subclasses.
2. **Variant (`src/rule_expr.hpp/.cpp`)**: the same six kinds as value
   types inside a `std::variant<...>` (`RuleExpr`). `evaluate()` and
   `describe()` dispatch with `std::visit` and a lambda overload set
   (`Overload`), no `RuleVisitor` interface and no `accept()` anywhere.

The test suite builds the identical logical tree both ways and asserts
`evaluate_classic()`/`evaluate()` and `describe_classic()`/`describe()`
agree for the same shipment, the behavior this chapter's prose claims
rather than merely asserts.

## Interpreter: walking the same tree to evaluate a grammar

`evaluate()` in `src/rule_expr.hpp/.cpp` is also this example's
Interpreter: it walks the AST to evaluate the small filter grammar it
represents. Nesting the same three leaf predicates two different ways
produces two different answers for the identical shipment, the way
operator precedence would for a parsed expression:
`(WeightAbove AND ZoneIs) OR DeclaredValueAbove` is `true` where
`WeightAbove AND (ZoneIs OR DeclaredValueAbove)` is `false`.

## Memento: snapshotting a whole draft rule set

`src/rule_set_editor.hpp/.cpp`'s `RuleSetEditor` is the Originator: a
merchant's in-progress rule set (draft name, named rules, a `published`
flag). `RuleSetEditor::Snapshot` is the Memento, a nested class with a
private constructor and private members, `RuleSetEditor` alone named as a
friend, so a caretaker can hold one and hand it back to `restore()` but
never read what is inside it. `save()` deep-clones every rule's `RuleExpr`
tree (`clone()` in `src/rule_expr.hpp/.cpp`, the same role Chapter 13's
Prototype plays for one object) so a snapshot cannot alias a tree the
live editor keeps mutating. `restore()` replaces the editor's entire
state in one call, distinct from Chapter 11's `CommandHistory`, which
reverses one `InventoryCommand` at a time.

### C++23 refinement (guarded)

`RuleSetEditor::try_evaluate_rule()` returns `std::expected<bool,
std::string>` instead of throwing, reporting a missing rule label as a
typed error. It is compiled only under C++23 (`#if __cplusplus >=
202302L` in `src/rule_set_editor.hpp` and `.cpp`), so the mandatory C++20
build path never needs `std::expected`. The matching gtest cases in
`tests/rule_set_editor_test.cpp` are guarded the same way.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/20-visitor-memento-interpreter
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23
cmake --build --preset cpp23
ctest --preset cpp23 --output-on-failure
```

It is also picked up by the repo root's superbuild, from the repo root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Either path discovers the same 20 gtest cases under `cpp23` (18 under
`cpp20`: the two `std::expected` cases are compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 20   # cpp23
100% tests passed, 0 tests failed out of 18   # cpp20
```

To see the classic and variant visitors agree, the interpreter's nesting
change the answer, and the memento restore a whole draft, build and run
the demo binary. From this directory, `./demo.sh` does the same steps
above plus runs it; or build just the demo target after a
`cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target visit20_demo
./build/cpp23/visit20_demo
```

Expected output:

```
== Visitor 1: classic double dispatch vs Visitor 2: std::variant + std::visit ==
Classic visitor:  match=true  (weight above 10.0kg AND zone is National)
Variant visitor:  match=true  (weight above 10.0kg AND zone is National)
Both representations agree on match and description.

== Interpreter: nesting changes the evaluated result ==
(weight above 10.0kg AND zone is National) OR declared value above $500.00
  -> true
weight above 10.0kg AND (zone is National OR declared value above $500.00)
  -> false
Same three predicates, different nesting, different answer for the identical shipment.

== Memento: snapshotting and restoring a whole draft rule set ==
Before edits:  name=Q4-international-filters rules=1 published=false
After edits:   name=DRAFT-EDIT rules=2 published=true
After restore: name=Q4-international-filters rules=1 published=false
Restored rule evaluates: true

== C++23 refinement: typed lookup error via std::expected ==
Rejected lookup: no such rule: ghost-rule
```

The last block only prints under the `cpp23` preset; under `cpp20` the
demo stops after the memento section.

## Files

```
examples/20-visitor-memento-interpreter/
  CMakeLists.txt        # standalone + superbuild-integrated targets
  CMakePresets.json      # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py            # standalone Conan recipe (gtest/1.17.0)
  demo.sh                 # build + run the demo binary
  src/
    shipping_domain.hpp/.cpp     # Shipment, Cents, DestinationZone, formatting helpers
    classic_visitor.hpp/.cpp     # Visitor expression 1: RuleNode hierarchy + RuleVisitor
    rule_expr.hpp/.cpp           # Visitor expression 2 + Interpreter: RuleExpr variant, evaluate(), describe(), clone()
    rule_set_editor.hpp/.cpp     # Memento: RuleSetEditor (Originator) + Snapshot
    demo_main.cpp                 # exercises all three patterns
  tests/
    classic_visitor_test.cpp
    rule_expr_test.cpp
    rule_set_editor_test.cpp
```

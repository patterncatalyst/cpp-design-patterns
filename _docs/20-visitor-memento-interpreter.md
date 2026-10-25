---
title: "Visitor, Memento, and Interpreter"
order: 20
part: "coordinating-traversing"
description: "Three patterns for a fixed shipping-filter rule tree: Visitor adds operations over the tree two idiomatic ways, the classic GoF double dispatch and a std::variant plus std::visit, Interpreter walks the same tree to evaluate a small filter grammar, and Memento snapshots and restores a whole draft rule set in one call."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Name the force Visitor answers: a fixed object structure that rarely gains a new node kind, paired with a growing set of operations that each need to touch every kind, without putting all of those operations inside the node classes themselves.
- Build a classic Gang of Four Visitor, a `RuleVisitor` interface and a class hierarchy with `accept()`, and show that a second operation ships with no change to any node class.
- Build the same structure and the same two operations with a `std::variant` of value types and `std::visit` over a lambda overload set, with no `RuleVisitor` interface and no `accept()` anywhere.
- Build an Interpreter that walks that structure to evaluate a small filter grammar, and show that the nesting of its And/Or/Not nodes decides the answer, the same way operator precedence decides the value of a parsed expression.
- Build a Memento that snapshots an object's entire state and restores it in one call, and say how that differs from Chapter 11's Command-based undo of one operation at a time.
- Recognize when any of the three patterns is answering a force the surrounding code does not have, and say what to write instead.

## Why this matters

A checkout platform that lets merchants define their own shipping filters, "hold any package over 20 kilograms for manual review," "flag international shipments with a declared value above $500," runs into the same small, closed grammar over and over: a predicate about one shipment attribute, combined with `AND`, `OR`, and `NOT`. The grammar itself barely changes once it is defined. What changes constantly is what a team wants to *do* with a filter once it exists: evaluate it against an incoming shipment, render it as a readable string for an audit log, eventually sum a surcharge it implies. That is the exact shape Visitor exists to handle: a fixed structure, a growing set of operations over it, and a design that wants the operations added without reopening every node class each time one ships.

A merchant editing one of these rule sets in an admin screen has a second problem with a different shape. Drafting a filter usually takes several steps: name the draft, add a rule, add another, maybe remove one that turned out wrong, flip a `published` flag when it is ready. If step four turns out to be a mistake, "edit the current state until it looks right again" is slower and more error-prone than "throw away everything since step three and start that step over." That is a whole-state rollback, not a single reversed operation, and it is the force Memento exists to handle: capture everything an object is, including the internals nobody outside it should be able to see, and hand back an opaque token that can restore all of it in one call.

Visitor's two idiomatic expressions in modern C++, the classic Gang of Four double dispatch and a `std::variant` with `std::visit`, both operate on the exact same rule-node shape this chapter builds, and Interpreter walks that identical shape to answer a third question: not "what string describes this rule" or "does this one predicate match," but "what does this whole filter expression evaluate to, given how its pieces are nested." Pairing the three patterns in one chapter is not an arbitrary grouping. All three operate on the same small AST, and telling them apart by what each one is *for*, adding an operation, interpreting a grammar, or snapshotting a state, is the same discipline Chapter 16 used to separate Decorator from Proxy by intent rather than by shape.

{% include excalidraw.html name="20-visitor-memento-interpreter" caption="Figure 20.1: A shared rule-node AST walked two ways, a classic double-dispatch Visitor and a std::variant plus std::visit Visitor producing the same result, with Interpreter walking the same AST to a grammar result; separately, a RuleSetEditor Originator whose Snapshot (Memento) captures and restores its whole state." %}

## The domain: filtering shipments, snapshotting a draft rule set

The example behind this chapter lives in `examples/20-visitor-memento-interpreter`, and it reuses this book's familiar `Shipment` shape from Chapter 8: a `weight_kg`, a `DestinationZone`, and a `declared_value_cents`. A rule against that shipment is one of three predicates, `WeightAbove`, `ZoneIs`, or `DeclaredValueAbove`, combined with `And`, `Or`, and `Not` into a small tree. That six-kind shape is the fixed structure every pattern in this chapter shares, expressed two different ways:

```cpp
// Classic form: a RuleNode class hierarchy (classic_visitor.hpp).
class RuleNode {
public:
    virtual ~RuleNode() = default;
    virtual void accept(RuleVisitor& visitor) const = 0;
};

class WeightAboveNode : public RuleNode { /* ... */ };
class ZoneIsNode : public RuleNode { /* ... */ };
class DeclaredValueAboveNode : public RuleNode { /* ... */ };
class AndNode : public RuleNode { /* left_, right_ */ };
class OrNode : public RuleNode { /* left_, right_ */ };
class NotNode : public RuleNode { /* inner_ */ };
```

```cpp
// Variant form: the same six kinds as value types (rule_expr.hpp).
struct WeightAbove { double threshold_kg; };
struct ZoneIs { DestinationZone zone; };
struct DeclaredValueAbove { Cents threshold_cents; };
struct AndExpr { std::unique_ptr<RuleExpr> left, right; };
struct OrExpr  { std::unique_ptr<RuleExpr> left, right; };
struct NotExpr { std::unique_ptr<RuleExpr> inner; };

struct RuleExpr {
    std::variant<WeightAbove, ZoneIs, DeclaredValueAbove, AndExpr, OrExpr, NotExpr> node;
};
```

Both shapes carry identical information: a leaf predicate checks one `Shipment` field against a threshold, and `And`/`Or`/`Not` combine other nodes. `And`/`Or`/`Not` own their children through `std::unique_ptr`, in both forms, the same RAII ownership Chapter 8's `RateEstimator` used for a single strategy; a `RuleExpr`'s `AndExpr` cannot outlive the tree it is part of, and nothing outside that tree can alias into the middle of it. The variant form needs `std::unique_ptr<RuleExpr>` for exactly one structural reason: `std::variant` cannot hold itself directly, so `AndExpr`, `OrExpr`, and `NotExpr` each hold their children behind a pointer, the smallest indirection a recursive variant can get away with.

`RuleSetEditor`, this chapter's Memento example, is a separate concern built on top of the variant form: a merchant's named, in-progress collection of `RuleExpr` trees, with a draft name and a `published` flag alongside them.

## Visitor, the classic shape: double dispatch over a fixed structure

The Gang of Four's original Visitor pairs a `RuleVisitor` interface with the class hierarchy above. Each concrete node implements `accept()` by calling `visitor.visit(*this)`, which resolves to the overload matching that node's exact type:

```cpp
class RuleVisitor {
public:
    virtual ~RuleVisitor() = default;
    virtual void visit(const WeightAboveNode& node) = 0;
    virtual void visit(const ZoneIsNode& node) = 0;
    virtual void visit(const DeclaredValueAboveNode& node) = 0;
    virtual void visit(const AndNode& node) = 0;
    virtual void visit(const OrNode& node) = 0;
    virtual void visit(const NotNode& node) = 0;
};
```

`accept()` resolves the node's exact type through ordinary virtual dispatch; `visit()` is already overloaded on that exact type, so by the time a `RuleVisitor` implementation's `visit(const AndNode&)` runs, it already knows precisely which node kind it is looking at, no `dynamic_cast` or type tag anywhere. That is the "double" in double dispatch: one virtual call picks the node's type, a second, already-resolved overload picks the operation's behavior for that type.

`MatchEvaluator` is the first concrete `RuleVisitor`, answering "does this shipment match the rule." Its `visit()` methods have no return type to hand a result back up through, so a recursive case like `AndNode` builds a fresh `MatchEvaluator` per child and reads each one's `result()` back:

```cpp
void MatchEvaluator::visit(const AndNode& node) {
    MatchEvaluator left_eval(shipment_);
    node.left().accept(left_eval);
    MatchEvaluator right_eval(shipment_);
    node.right().accept(right_eval);
    result_ = left_eval.result() && right_eval.result();
}
```

`RuleDescriber` is the second `RuleVisitor`, rendering a rule as a readable string for an audit log, `(weight above 10.0kg AND zone is National)`. Adding it costs exactly one new class; `RuleNode` and all six of its subclasses are untouched. That is Visitor's whole argument in one sentence: a structure that rarely grows a new node kind can absorb an unbounded number of new operations, each one a new `RuleVisitor` implementation, without a single edit to the node classes an operation runs over.

## Visitor, the modern shape: std::variant and std::visit

The variant form answers the identical force without a `RuleVisitor` interface or an `accept()` method anywhere. `std::visit` dispatches directly on `RuleExpr::node`'s active alternative, and a lambda-per-alternative overload set reads as one operation with one branch per node kind:

```cpp
template <typename... Alternatives>
struct Overload : Alternatives... {
    using Alternatives::operator()...;
};
template <typename... Alternatives>
Overload(Alternatives...) -> Overload<Alternatives...>;

bool evaluate(const RuleExpr& expr, const Shipment& shipment) {
    return std::visit(
        Overload{
            [&](const WeightAbove& p) { return shipment.weight_kg > p.threshold_kg; },
            [&](const ZoneIs& p) { return shipment.zone == p.zone; },
            [&](const DeclaredValueAbove& p) { return shipment.declared_value_cents > p.threshold_cents; },
            [&](const AndExpr& e) { return evaluate(*e.left, shipment) && evaluate(*e.right, shipment); },
            [&](const OrExpr& e) { return evaluate(*e.left, shipment) || evaluate(*e.right, shipment); },
            [&](const NotExpr& e) { return !evaluate(*e.inner, shipment); },
        },
        expr.node);
}
```

`describe()` is the second operation, built the same way: one more `std::visit` call with six more lambdas, added with no change to `WeightAbove`, `ZoneIs`, `DeclaredValueAbove`, `AndExpr`, `OrExpr`, or `NotExpr`. `examples/20-visitor-memento-interpreter`'s test suite builds the identical logical tree both ways and asserts `evaluate_classic()` and `evaluate()` agree, and that `describe_classic()` and `describe()` produce the identical string, for the same shipment every time. Neither form is "more correct" Visitor; a `RuleVisitor` interface costs a vtable per node and buys an open set of node kinds a plugin could add later, while a `std::variant` costs nothing per call and buys exhaustiveness at compile time: a `std::visit` call must supply all six alternatives in its overload set to compile, the identical completeness guarantee a `RuleVisitor` subclass gets from `RuleVisitor` declaring every `visit()` overload pure virtual.

## Interpreter: walking the same tree to evaluate a grammar

Interpreter is not a third way to write Visitor's operations. It names a different force: a small grammar, here three predicate kinds combined with `And`, `Or`, and `Not`, and a canonical way to evaluate an expression written in that grammar. `evaluate()` above already is that canonical evaluator; this chapter's Interpreter and its variant-form Visitor sit on literally the same function, which is the point worth sitting with rather than glossing over. Visitor's question is "what operations can I add over this fixed structure"; Interpreter's question is "given this structure already *is* a grammar, what does walking it compute." The same `std::visit` recursion answers both, because in modern C++ a tree-walking interpreter over an AST and a Visitor operation over that same AST are the identical mechanism aimed at two different design questions.

What makes Interpreter worth its own section rather than a footnote on Visitor is nesting. Building expressions through this chapter's `rule_and()`, `rule_or()`, and `rule_not()` helpers makes nesting an explicit, visible choice, the same three leaf predicates assembled in a different shape producing a different answer:

```cpp
RuleExpr outer_or = rule_or(
    rule_and(weight_above(10.0), zone_is(DestinationZone::National)),
    declared_value_above(50000));

RuleExpr outer_and = rule_and(
    weight_above(10.0),
    rule_or(zone_is(DestinationZone::National), declared_value_above(50000)));
```

Evaluated against a 5.0 kilogram shipment headed to `National` with a declared value of $800, `outer_or` is `true`: the weight predicate fails, but `OR`-ing in `DeclaredValueAbove(50000)` still matches. `outer_and` is `false`: the same weight predicate failing now gates the entire `AND`, and the `OR` on its right side never gets a chance to rescue the result. Three identical leaf predicates, two different trees, two different answers for the identical shipment. That is precedence, expressed as tree shape instead of as operator-binding rules a parser would otherwise need to apply; building the tree directly, the way this chapter's helper functions do, sidesteps writing a parser at all while keeping the exact behavior a parsed grammar would have.

## Memento: snapshotting a whole draft, not one edit at a time

`RuleSetEditor` is this chapter's Originator: a merchant's in-progress rule set, a draft name, a `published` flag, and a list of named `RuleExpr` trees built up across several editing steps. Its `Snapshot` is the Memento, a nested class whose constructor and data members are private, with `RuleSetEditor` alone named as a friend:

```cpp
class RuleSetEditor {
public:
    class Snapshot {
    public:
        Snapshot(Snapshot&&) = default;
        Snapshot(const Snapshot&) = delete;
    private:
        friend class RuleSetEditor;
        Snapshot(std::string draft_name, std::vector<NamedRule> rules, bool published);
        std::string draft_name_;
        std::vector<NamedRule> rules_;
        bool published_;
    };

    Snapshot save() const;
    void restore(Snapshot snapshot);
    /* ... */
};
```

A caretaker, the admin screen's controller code in this example, can hold a `Snapshot`, move it into a local variable, and hand it back to `restore()` later, but it cannot read a single field inside one; the friendship that would let it do so is granted to `RuleSetEditor` and nobody else. That is Memento's defining property, stated precisely rather than loosely: capture an object's state in a form a caretaker can store and return, without exposing that state to the caretaker along the way.

{% include excalidraw.html name="20-memento-save-restore" caption="Figure 20.2: RuleSetEditor saves its whole state into an opaque Snapshot that a caretaker can hold and return, and restore() replaces that state in one call." %}

`save()` does one thing beyond copying three fields: it deep-clones every rule's `RuleExpr` tree with the `clone()` function this chapter's variant form also ships, the same role Chapter 13's Prototype played for a single concrete object.

```cpp
RuleSetEditor::Snapshot RuleSetEditor::save() const {
    std::vector<NamedRule> cloned_rules;
    for (const NamedRule& rule : rules_) {
        cloned_rules.push_back(NamedRule{rule.label, clone(rule.expr)});
    }
    return Snapshot(draft_name_, std::move(cloned_rules), published_);
}
```

Without that clone, a `Snapshot` would hold pointers into the same `RuleExpr` trees the live `RuleSetEditor` keeps mutating, and "restoring" an old snapshot would hand back whatever those trees had become by the time `restore()` ran, not what they were at `save()` time. `restore()` is the mirror operation: it replaces `draft_name_`, `rules_`, and `published_` wholesale, in one call, with whatever the `Snapshot` captured.

Memento differs from Chapter 11's Command-based undo in one specific way. `CommandHistory` in Chapter 11 reverses one `InventoryCommand` at a time, walking backward through a stack of operations that each know their own inverse; undoing three operations means running three inverses in sequence, and anything in between is still visible to a caller who only asked to undo once. `RuleSetEditor::restore()` does not know or care how many `add_rule()`, `remove_rule()`, or `set_published()` calls happened since `save()`; it replaces the whole object in a single assignment, as if none of them had ever run. Reach for Command's per-operation undo when an operator needs to step backward and forward through a sequence of discrete actions, one at a time, the way a text editor's Ctrl-Z does. Reach for Memento when a caller wants to discard an entire batch of changes in one call and does not care how many changes that batch contained.

> **AI sidebar.** Ask an LLM pairing partner to "let me undo editing this rule set" and a common first answer is Command: wrap every `add_rule()` and `set_published()` call in an `InventoryCommand`-shaped object with its own `undo()`. That is the right shape for Chapter 11's warehouse editor, where each operation has a natural, cheap inverse. It is the wrong shape here, where the request is "go back to how the whole draft looked a minute ago," not "reverse my last five edits one at a time." Writing a bespoke inverse for `add_rule()` (remove the rule just added), `remove_rule()` (re-add whatever was removed, in its original position, with its original tree), and `set_published()` (restore the prior flag) is real, fiddly code that a whole-state `Snapshot` sidesteps entirely by never needing an inverse for anything. Before accepting a generated Command-undo stack, ask whether the request is "reverse my last operation" or "throw away everything since this point," because the two map to different patterns in this book, Command and Memento, with different amounts of code behind each one.

## A typed rule lookup: C++20 versus C++23

`RuleSetEditor::evaluate_rule()` looks a named rule up by label and throws `std::out_of_range` when the label is missing, which works but forces every caller either to wrap the lookup in a `try` block or accept that an unhandled exception can propagate out of a filter evaluation. C++23's `std::expected` gives the same lookup a typed alternative: a return type that carries either the match result or a specific reason the lookup failed, checkable with an `if` and no exception machinery at all. `try_evaluate_rule()` in this example is guarded behind `#if __cplusplus >= 202302L` precisely so the mandatory C++20 build path never needs `std::expected`, while the C++23 build gets the more precise contract.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: throwing on a missing label works, but it forces every caller
// to wrap the lookup in a try block or accept an unhandled exception
// propagating out of a filter evaluation.
bool RuleSetEditor::evaluate_rule(const std::string& label, const Shipment& shipment) const {
    const NamedRule* rule = find_rule(label);
    if (rule == nullptr) {
        throw std::out_of_range("no such rule: " + label);
    }
    return evaluate(rule->expr, shipment);
}
```
```cpp
// C++23: std::expected reports the same missing-label failure as a typed
// error, checkable with an if and no exception machinery.
std::expected<bool, std::string> RuleSetEditor::try_evaluate_rule(
    const std::string& label, const Shipment& shipment) const {
    const NamedRule* rule = find_rule(label);
    if (rule == nullptr) {
        return std::unexpected<std::string>("no such rule: " + label);
    }
    return evaluate(rule->expr, shipment);
}
```

Both versions look the same label up against the identical set of named rules. Only how a caller learns about a missing one changes between the two columns, the same narrow scope this book's codetabs convention has held to since Chapter 8's typed Strategy error and Chapter 16's typed Proxy access check.

## When not to use Visitor, Interpreter, or Memento

All three patterns in this chapter earn their cost when the force behind them is real and not merely anticipated. Outside that, each one adds a layer with nothing underneath it to justify the cost.

Visitor pays for itself when a structure stays fixed and the operations over it multiply. A rule tree with exactly one operation, "does this shipment match," evaluated in exactly one place, needs neither a `RuleVisitor` hierarchy nor a `std::variant` overload set; it needs the handful of `if`/`else if` branches a single function can hold, read start to finish by one reviewer. The moment that changes is the moment a second operation, a third, or a plugin-supplied node kind becomes a real requirement, not before: introducing either Visitor shape ahead of a second operation is this chapter's version of the branch-inside-the-method trap Chapter 16 described for a premature Decorator, a vtable or a template instantiation paid for before anything needs it.

Interpreter pays for itself when a grammar is open-ended, built from user input or configuration rather than fixed at compile time by the handful of call sites that use it. A single hard-coded filter condition, checked once in checkout code with no merchant-facing editor anywhere near it, does not need an AST, a `RuleExpr` tree, or an `evaluate()` walking it; it needs a plain boolean expression written directly where it is checked. Building an interpreter for a grammar with one fixed expression and no second expression coming is solving a parsing-and-evaluation problem nobody has.

Memento pays for itself when a whole-state rollback is a real requirement, not a hypothetical one. An object with no multi-step editing workflow, no "discard this batch of changes" operation a caller ever asks for, does not need a `Snapshot` nested class and a `save()`/`restore()` pair sitting unused beside it; it needs to be mutated directly, with no undo path at all until somebody asks for one. The sharper trap is reaching for Memento when Command's per-operation undo is what the problem calls for, cloning an entire object on every edit to support undoing one keystroke, when a single cheap inverse operation would do the same job for a fraction of the cost. Name which rollback shape a caller needs, one step back or one whole batch discarded, before building either pattern's machinery.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/20-visitor-memento-interpreter` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

This example is also standalone-buildable on its own; the run script there builds and runs it, and its `README.md` covers what it does and how to drive it:

```sh
cd examples/20-visitor-memento-interpreter
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23
cmake --build --preset cpp23
ctest --preset cpp23 --output-on-failure
```

`scripts/verify.sh` (or `ctest --preset <preset>` run directly from this example's own directory) discovers this example's GoogleTest cases through `gtest_discover_tests`: 20 cases under the `cpp23` preset, 18 under `cpp20`, the difference being the two `std::expected`-specific cases that only compile under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 20   # cpp23
100% tests passed, 0 tests failed out of 18   # cpp20
```

To see the classic and variant visitors agree, the interpreter's nesting change the answer, and the memento restore a whole draft, run the example's own demo script:

```sh
examples/20-visitor-memento-interpreter/demo.sh cpp23
```

It evaluates one rule two ways, evaluates two differently-nested trees built from the same three predicates, and walks a draft rule set through an edit-then-restore cycle:

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

The first block's two "match=true" lines, printed from two independently implemented visitors walking two independently built trees, are the classic-versus-variant agreement this chapter's test suite also asserts. The second block's "true" then "false", computed from the identical three predicates nested two different ways, is nesting deciding an interpreted grammar's answer, visible at the terminal rather than only in an assertion. The memento block's final line, "rules=1" again after "rules=2" mid-edit, and the restored rule still evaluating correctly against the same shipment, is a whole batch of edits discarded in one `restore()` call, not reversed one at a time. Running `examples/20-visitor-memento-interpreter/demo.sh cpp20` prints the same output minus the final `std::expected` block, which only compiles under the `cpp23` preset.

## For deeper coverage

Visitor, Interpreter, and Memento are the last three patterns in the Gang of Four's original 23. Every chapter from here back to Chapter 2 has now worked through one, several, or, in this chapter's case, three of them, each against new, real-domain C++20/23 code rather than the book's own illustrations.

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Visitor, Interpreter, and Memento entries, including the "represent an operation to be performed on the elements of an object structure," "given a language, define a representation for its grammar," and "capture and externalize an object's internal state" intent statements this chapter's rule tree, grammar walk, and draft snapshot each build on.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/20-visitor-memento-interpreter`'s suite, which asserts that two independently built visitors agree on one tree, that nesting order changes an interpreted result, and that a restored snapshot evaluates identically to the state it was taken from, rather than only checking that each class constructs without error.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.

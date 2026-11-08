---
title: "Visitor, Memento, and Interpreter"
order: 20
part: "coordinating-traversing"
description: "Three patterns for a fixed shipping-filter rule tree: Visitor adds operations over the tree two idiomatic ways, the classic GoF double dispatch and a std::variant plus std::visit, Interpreter walks the same tree to evaluate a small filter grammar, and Memento snapshots and restores a whole draft rule set in one call."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Add operations over a fixed object structure without editing its node classes.
- Build a classic Visitor, a `RuleVisitor` interface over a class hierarchy with `accept()`, and ship a second operation with no node class changed.
- Build the same structure and operations as a `std::variant` of value types with `std::visit` over a lambda overload set.
- Build an Interpreter that evaluates a small filter grammar, and show that the nesting of And/Or/Not decides the answer.
- Snapshot an object's entire state and restore it in one call, and say how that differs from per-operation Command undo.
- Recognize when the surrounding code doesn't have the force any of the three answers.

## Why this matters

Merchants define their own shipping filters: "hold any package over 20 kilograms for manual review", "flag international shipments declared above $500". Every filter is a predicate about one shipment attribute combined with `AND`, `OR` and `NOT`. That grammar barely changes. What changes is what a team does with a filter: evaluate it against a shipment, render it for an audit log, later sum a surcharge it implies.

Editing a rule set takes several steps: name the draft, add a rule, add another, remove one, flip `published`. When step four is a mistake, editing until the state looks right again is slower and riskier than discarding everything since step three.

Visitor adds operations over the rule tree, Interpreter evaluates the grammar the tree represents, and Memento rolls the whole draft back.

{% include excalidraw.html name="20-visitor-memento-interpreter" caption="Figure 20.1: A shared rule-node AST walked two ways, a classic double-dispatch Visitor and a std::variant plus std::visit Visitor producing the same result, with Interpreter walking the same AST to a grammar result; separately, a RuleSetEditor Originator whose Snapshot (Memento) captures and restores its whole state." %}

A rule tests a `Shipment` (`weight_kg`, a `DestinationZone`, `declared_value_cents`) with one of three predicates, `WeightAbove`, `ZoneIs` or `DeclaredValueAbove`, combined by `And`, `Or` and `Not`. Those six kinds are the fixed structure.

## Pattern: Visitor

### The problem

Evaluating, describing and pricing a rule each need a branch per node kind. Putting every operation inside the node classes means reopening all six classes each time an operation ships.

### The solution

**A Visitor represents an operation over the elements of an object structure as a separate object, so new operations are added without changing the element classes.**

### How C++ models it: double dispatch

In the classic form, every node derives from `RuleNode`:

```cpp
class RuleNode {
public:
    virtual ~RuleNode() = default;
    virtual void accept(RuleVisitor& visitor) const = 0;
};
```

`WeightAboveNode`, `ZoneIsNode`, `DeclaredValueAboveNode`, `AndNode`, `OrNode` and `NotNode` each implement `accept()` as `visitor.visit(*this)`. The visitor interface has one overload per kind:

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

The virtual `accept()` picks the node's type, and the overload it calls picks the operation's behavior for that type, with no `dynamic_cast` or type tag. That's the double in double dispatch.

`MatchEvaluator` answers "does this shipment match". `visit()` returns nothing, so a recursive case builds a fresh evaluator per child and reads its result:

```cpp
void MatchEvaluator::visit(const AndNode& node) {
    MatchEvaluator left_eval(shipment_);
    node.left().accept(left_eval);
    MatchEvaluator right_eval(shipment_);
    node.right().accept(right_eval);
    result_ = left_eval.result() && right_eval.result();
}
```

`RuleDescriber` renders a rule for the audit log, `(weight above 10.0kg AND zone is National)`. Adding it costs one class; `RuleNode` and its six subclasses are untouched.

### How C++ models it: `std::variant` and `std::visit`

The same six kinds as value types:

```cpp
struct WeightAbove {
    double threshold_kg;
};

struct ZoneIs {
    DestinationZone zone;
};

struct DeclaredValueAbove {
    Cents threshold_cents;
};

struct RuleExpr;

// And/Or/Not hold their children behind unique_ptr<RuleExpr> because a
// std::variant cannot hold itself directly; the pointer is the only
// indirection a recursive variant needs, and it still owns its children
// outright, the same RAII ownership Chapter 8's RateEstimator used for a
// single strategy.
struct AndExpr {
    std::unique_ptr<RuleExpr> left;
    std::unique_ptr<RuleExpr> right;
};

struct OrExpr {
    std::unique_ptr<RuleExpr> left;
    std::unique_ptr<RuleExpr> right;
};

struct NotExpr {
    std::unique_ptr<RuleExpr> inner;
};

struct RuleExpr {
    std::variant<WeightAbove, ZoneIs, DeclaredValueAbove, AndExpr, OrExpr, NotExpr> node;
};
```

There's no `RuleVisitor` and no `accept()`. A lambda-per-alternative overload set, deduced by class template argument deduction, makes a `std::visit` call read as one operation with one branch per kind:

```cpp
template <typename... Alternatives>
struct Overload : Alternatives... {
    using Alternatives::operator()...;
};

template <typename... Alternatives>
Overload(Alternatives...) -> Overload<Alternatives...>;
```

```cpp
bool evaluate(const RuleExpr& expr, const Shipment& shipment) {
    return std::visit(
        Overload{
            [&](const WeightAbove& predicate) { return shipment.weight_kg > predicate.threshold_kg; },
            [&](const ZoneIs& predicate) { return shipment.zone == predicate.zone; },
            [&](const DeclaredValueAbove& predicate) {
                return shipment.declared_value_cents > predicate.threshold_cents;
            },
            [&](const AndExpr& expr_and) {
                return evaluate(*expr_and.left, shipment) && evaluate(*expr_and.right, shipment);
            },
            [&](const OrExpr& expr_or) {
                return evaluate(*expr_or.left, shipment) || evaluate(*expr_or.right, shipment);
            },
            [&](const NotExpr& expr_not) { return !evaluate(*expr_not.inner, shipment); },
        },
        expr.node);
}
```

`describe()` is one more `std::visit` with six more lambdas, and no value type changes. Built the same logical tree both ways, the classic and variant forms agree on every match and produce identical descriptions.

### Interface or variant

| | `RuleVisitor` hierarchy | `std::variant` + `std::visit` |
| --- | --- | --- |
| Dispatch cost | Two virtual calls per node | A visit on the active alternative, no vtable |
| Node kinds | Open: a plugin can add a subclass, and every visitor must learn it | Closed at compile time |
| Missing case | Won't instantiate: every `visit()` is pure virtual | Won't compile: the overload set must cover all six |
| Nodes | Heap objects behind pointers | Values, with pointers only for children |

## Pattern: Interpreter

### The problem

The filter language is a grammar: three predicates combined with `And`, `Or` and `Not`. A rule written in it needs one canonical meaning, and that meaning depends on how the pieces nest.

### The solution

**An Interpreter defines a representation for a grammar and evaluates sentences in it by walking that representation.**

### How C++ models it

`evaluate()` above is the interpreter. Visitor asks "what operations can I add over this structure"; Interpreter asks "this structure is a grammar, so what does walking it compute". In modern C++ the same `std::visit` recursion answers both.

### Nesting is precedence

The `rule_and()`, `rule_or()` and `rule_not()` helpers make nesting explicit. The same three predicates in two shapes:

```cpp
    rules::RuleExpr outer_or =
        rules::rule_or(rules::rule_and(rules::weight_above(10.0), rules::zone_is(DestinationZone::National)),
                        rules::declared_value_above(50000));
    rules::RuleExpr outer_and = rules::rule_and(
        rules::weight_above(10.0),
        rules::rule_or(rules::zone_is(DestinationZone::National), rules::declared_value_above(50000)));
```

For a 5.0 kg National shipment declared at $800:

| Tree | Result | Why |
| --- | --- | --- |
| `(weight > 10 AND National) OR value > $500` | `true` | The weight fails, but the `OR` matches on value |
| `weight > 10 AND (National OR value > $500)` | `false` | The failing weight gates the whole `AND` |

Precedence is expressed as tree shape instead of operator-binding rules. Building the tree directly skips writing a parser and keeps the behavior a parsed grammar would have.

## Pattern: Memento

### The problem

A merchant wants to discard a batch of edits to a draft rule set, however many there were, and return to a known state. The draft's internals shouldn't be visible to the code that holds the saved state.

### The solution

**A Memento captures an object's internal state in an opaque token that a caretaker can store and hand back, without exposing that state to the caretaker.**

### How C++ models it

`RuleSetEditor` is the originator: a draft name, a `published` flag and a list of named `RuleExpr` trees. Its nested `Snapshot` is the memento, with private data and a private constructor, and only `RuleSetEditor` as a friend:

```cpp
    class Snapshot {
    public:
        Snapshot(Snapshot&&) = default;
        Snapshot& operator=(Snapshot&&) = default;
        Snapshot(const Snapshot&) = delete;
        Snapshot& operator=(const Snapshot&) = delete;

    private:
        friend class RuleSetEditor;
        Snapshot(std::string draft_name, std::vector<NamedRule> rules, bool published);

        std::string draft_name_;
        std::vector<NamedRule> rules_;
        bool published_;
    };
```

The caretaker, an admin screen's controller, can hold a `Snapshot`, move it and pass it back to `restore()`, but can't read a field of it.

{% include excalidraw.html name="20-memento-save-restore" caption="Figure 20.2: RuleSetEditor saves its whole state into an opaque Snapshot that a caretaker can hold and return, and restore() replaces that state in one call." %}

`save()` deep-clones every rule's tree with `clone()`:

```cpp
RuleSetEditor::Snapshot RuleSetEditor::save() const {
    std::vector<NamedRule> cloned_rules;
    cloned_rules.reserve(rules_.size());
    for (const NamedRule& rule : rules_) {
        cloned_rules.push_back(NamedRule{rule.label, clone(rule.expr)});
    }
    return Snapshot(draft_name_, std::move(cloned_rules), published_);
}
```

A snapshot that shared trees with the live editor would restore whatever those trees had become, not what they were at `save()` time. `restore()` replaces `draft_name_`, `rules_` and `published_` in one call.

### Memento or Command undo

| | Command undo | Memento |
| --- | --- | --- |
| Unit | One operation, with its own inverse | The whole object state |
| Going back three steps | Three inverses in sequence | One `restore()` |
| Code per operation | An inverse for each | None |
| Cost | Small per step | A full copy per snapshot |
| Fits | Step-by-step undo and redo, like Ctrl-Z | Discard a batch of edits since a known point |

### A typed rule lookup

`evaluate_rule()` throws `std::out_of_range` for a missing label, so every caller wraps it in `try` or lets the exception leave filter evaluation. `std::expected`, new in C++23, reports the failure as a value. `try_evaluate_rule()` sits behind `#if __cplusplus >= 202302L`, so the C++20 build never sees it.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
bool RuleSetEditor::evaluate_rule(const std::string& label, const Shipment& shipment) const {
    const NamedRule* rule = find_rule(label);
    if (rule == nullptr) {
        throw std::out_of_range("no such rule: " + label);
    }
    return evaluate(rule->expr, shipment);
}
```
```cpp
std::expected<bool, std::string> RuleSetEditor::try_evaluate_rule(const std::string& label,
                                                                    const Shipment& shipment) const {
    const NamedRule* rule = find_rule(label);
    if (rule == nullptr) {
        return std::unexpected<std::string>("no such rule: " + label);
    }
    return evaluate(rule->expr, shipment);
}
```

Both look up the same rules. Only how a caller learns about a missing label changes.

## When not to use it

- **Visitor.** A rule tree with one operation evaluated in one place needs a few branches in one function, not a `RuleVisitor` hierarchy or an overload set. Add a visitor when a second operation, or a plugin-supplied node kind, is a real requirement.
- **Interpreter.** Build one when the grammar is open-ended, assembled from user input or configuration. A single hard-coded condition in checkout code is a boolean expression written where it's checked.
- **Memento.** An object with no multi-step editing workflow and no "discard this batch" request needs no `Snapshot`. And when the request is to undo one keystroke at a time, cloning the whole object on every edit costs far more than one cheap inverse; use Command.

<blockquote class="ai-sidebar" markdown="1">
**Specifying rule-set undo.** "Let me undo editing this rule set" leaves open whether the undo reverses one operation or discards everything since a point, and whether saved state is readable from outside. State it:

> Add whole-state undo to `RuleSetEditor` as a Memento. `save()` returns a nested `RuleSetEditor::Snapshot` with private members, a private constructor and `RuleSetEditor` as its only friend; it is move-only. `save()` deep-copies every rule with `clone(rule.expr)`, and `restore(Snapshot)` replaces `draft_name_`, `rules_` and `published_` in one call. Don't write per-operation inverses for `add_rule()`, `remove_rule()` or `set_published()`.

Run `ctest --preset cpp23 -R RuleSetEditor`, and check that no public accessor on `Snapshot` exposes its fields.
</blockquote>

## Build, run, observe

From the repository root, the superbuild picks up `examples/20-visitor-memento-interpreter`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

It also builds standalone from its own directory; its `README.md` covers driving it:

```sh
cd examples/20-visitor-memento-interpreter
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23
cmake --build --preset cpp23
ctest --preset cpp23 --output-on-failure
```

All 20 cases pass under `cpp23` and 18 under `cpp20`; the two `std::expected` cases compile only under C++23. To see the visitors agree, the nesting change the answer and the memento restore a draft:

```sh
examples/20-visitor-memento-interpreter/demo.sh cpp23
```

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

Two separately built visitors agree, the same predicates nested two ways give `true` and `false`, and one `restore()` discards a rename, an added rule and a publish. Under `cpp20` the output stops before the `std::expected` block.

- **Drop a case from the overload set.** In a scratch copy, delete the `NotExpr` lambda from `evaluate()` in `src/rule_expr.cpp`. The build fails inside `std::visit` with `no type named 'type' in 'std::invoke_result<...>'` for `const rules::NotExpr &`.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): the original Visitor, Interpreter, and Memento entries, including the "represent an operation to be performed on the elements of an object structure," "given a language, define a representation for its grammar," and "capture and externalize an object's internal state" intent statements.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/20-visitor-memento-interpreter`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).

---
title: "Expression templates and generic idioms"
order: 23
part: "idiomatic-cpp-expressions"
description: "Expression templates that build a compile-time expression tree so a+b+c evaluates in one pass with no intermediate objects, plus two supporting idioms from the same freight-route domain: a self-registering Prototype registry and a single describe/visit hook that drives both serialize() and deserialize()."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Explain why `a + b + c` written against ordinary operator overloads builds one throwaway object per operator, and how an expression template avoids that by deferring evaluation until the whole expression is assigned.
- Build a small expression-template type with a CRTP base, two node types for "sum" and "scaled," and a single converting constructor that is the only place a full result object gets built.
- Prove the no-temporaries claim with a concrete count, not a visual read of the code, and confirm the lazy result equals the eager one.
- Build a self-registering Prototype registry where a concrete type registers itself from its own translation unit, with no central list or switch to edit when a new type arrives.
- Build a templated serialization format driven by one describe/visit hook, so `serialize()` and `deserialize()` stay in sync by construction instead of by discipline.
- Recognize when an expression template is solving a problem a plain loop already solves, and say what the simpler code costs and saves.

## Why this matters

A freight dispatcher closing out a route report needs a weighted combination of several legs' metrics: distance covered, fuel burned, dwell time at each handoff dock. The formula is simple, something like a distance-weighted average across three legs of a single shipment's journey, but the naive way to write it in C++ pays a measurable cost that has nothing to do with the arithmetic itself. Every `+` and every `*` in `0.5 * leg_one + 0.3 * leg_two + 0.2 * leg_three`, written against ordinary operator overloads that each return a new object by value, builds a fresh, fully-constructed intermediate object that exists only long enough to feed the next operator. For a three-term sum that is five throwaway objects built and discarded to produce one answer.

Expression templates are the idiom C++ developed to make that cost optional. Instead of each operator doing the arithmetic immediately and returning a concrete value, each operator returns a small, cheap object that only *describes* the arithmetic: "this is `0.5` times that other thing." Nothing is computed yet. Only when the whole chain of operators is finally assigned to a real variable does anything run, and it runs once, in a single pass over the thing's fields, producing exactly one object instead of five. This chapter builds that mechanism from the ground up against a `RouteMetrics` type, then adds two more idioms this book already introduced at the pattern level and treats here at the idiom level: Chapter 13's hand-registered `PackageTemplateRegistry` becomes a registry concrete types register themselves into, and Chapter 13's hand-written `serialize_manifest()`/`deserialize_manifest()` pair becomes two generic functions driven by a single list of fields.

{% include excalidraw.html name="23-expression-templates" caption="Figure 23.1: an expression template defers 0.5 x leg_one + 0.3 x leg_two + 0.2 x leg_three into a tree of RouteScaled/RouteSum nodes that evaluates once into a RouteMetrics, against the eager version that builds a new NaiveRouteMetrics object at every operator." %}

## The domain: weighting a multi-leg freight route

The example behind this chapter lives in `examples/23-expression-templates`. A `RouteMetrics` carries three physical measurements for one leg of a route:

```cpp
class RouteMetrics : public RouteExpr<RouteMetrics> {
public:
    RouteMetrics(double distance_km, double fuel_liters, double dwell_hours);

    double distance_km() const { return values_[0]; }
    double fuel_liters() const { return values_[1]; }
    double dwell_hours() const { return values_[2]; }

private:
    std::array<double, 3> values_;
};
```

`RouteMetrics` is unremarkable as a value type, three doubles behind named accessors. What makes it the subject of this chapter is the base class it derives from, `RouteExpr<RouteMetrics>`, and the operators that work against that base rather than against `RouteMetrics` directly. `RouteExpr<Derived>` is a CRTP base, the same static-polymorphism shape Chapter 21 introduced, and it supplies exactly one thing: a typed cast back to the concrete node. Every node type in this chapter's expression tree, `RouteMetrics` itself, and the two node types below, derives from `RouteExpr` and gets combined by the same two operators.

## Expression templates: evaluating a weighted sum in one pass

Two small node types carry the deferred arithmetic. `RouteSum` remembers "left plus right" without computing it, and `RouteScaled` remembers "factor times expression" the same way:

```cpp
template <typename Left, typename Right>
class RouteSum : public RouteExpr<RouteSum<Left, Right>> {
public:
    RouteSum(const Left& lhs, const Right& rhs) : lhs_(lhs), rhs_(rhs) {}
    double operator[](std::size_t index) const { return lhs_[index] + rhs_[index]; }
private:
    const Left& lhs_;
    const Right& rhs_;
};

template <typename Expr>
class RouteScaled : public RouteExpr<RouteScaled<Expr>> {
public:
    RouteScaled(double factor, const Expr& expr) : factor_(factor), expr_(expr) {}
    double operator[](std::size_t index) const { return factor_ * expr_[index]; }
private:
    double factor_;
    const Expr& expr_;
};
```

Neither class does any arithmetic in its constructor. Both hold references to whatever they were built from, `RouteSum` to its two operands and `RouteScaled` to the expression it scales, and both defer the actual multiply or add to `operator[]`, which runs once per field, only when something asks for a field's value. `operator+` and `operator*` build these nodes instead of a `RouteMetrics`:

```cpp
template <typename Left, typename Right>
RouteSum<Left, Right> operator+(const RouteExpr<Left>& lhs, const RouteExpr<Right>& rhs) {
    return RouteSum<Left, Right>(lhs.derived(), rhs.derived());
}

template <typename Expr>
RouteScaled<Expr> operator*(double factor, const RouteExpr<Expr>& expr) {
    return RouteScaled<Expr>(factor, expr.derived());
}
```

Write `0.5 * leg_one + 0.3 * leg_two + 0.2 * leg_three` against these operators and nothing gets computed yet. The compiler builds a small, fully-typed tree: three `RouteScaled` nodes, then a `RouteSum` combining the first two, then a second `RouteSum` combining that result with the third `RouteScaled`. Every one of those five nodes is a handful of bytes on the stack, a reference and maybe a `double`, with no arithmetic run inside any of them. The only place arithmetic happens, and the only place a complete `RouteMetrics` gets built, is `RouteMetrics`'s templated converting constructor:

```cpp
template <typename Expr>
RouteMetrics(const RouteExpr<Expr>& expr)
    : values_{expr.derived()[0], expr.derived()[1], expr.derived()[2]} {
    ++g_construction_count;
}
```

Assigning the five-node expression tree to a `RouteMetrics` variable runs this one constructor exactly once. It asks the root of the tree for each of the three field values, and the tree answers by recursively asking its own operands, all the way down to the three original `RouteMetrics` legs, all within this single call. One pass, three field reads each touching every node once, one object built.

The payoff is concrete, not a claim about compilers being clever. `examples/23-expression-templates` keeps a shared construction counter and a second type built only for the comparison, `NaiveRouteMetrics`, whose `operator+` and `operator*` are ordinary overloads that each return a brand-new object immediately. Running the identical weighted sum against both types and reading the counter after each one shows the lazy path constructing exactly one `RouteMetrics`, while the eager path constructs five `NaiveRouteMetrics` objects, three from the scalar multiplies and two from the adds, to reach the same answer. The test suite in `tests/expression_template_test.cpp` asserts both halves of that claim directly: the two results compare equal field for field, and the lazy construction count is `1` while the eager one is `5`. That is laziness proven by a number at test time, not inferred from reading the operator definitions and hoping the optimizer agrees.

That proof matters because the safety story here has a real cost attached to it. `RouteSum` and `RouteScaled` hold references, not copies, which is exactly what keeps them cheap: no node ever copies the `RouteMetrics` values it was built from. The price is that those references are only valid for as long as the expression they describe is still being evaluated within the same full statement. Naming an intermediate expression with `auto` and reusing it on a later line is the classic way this idiom turns dangerous: `auto partial = 0.5 * leg_one;` looks like it holds a scaled copy of `leg_one`, but it holds a reference back to `leg_one` wrapped in a `RouteScaled`, and if `leg_one` is itself a temporary that already went out of scope, `partial` is a dangling reference nobody's type system flags. The fix is the discipline this chapter's example already follows everywhere: build an expression and assign it to a concrete `RouteMetrics` in the same statement, never store the intermediate expression type itself. That discipline, plus the compile-time template error messages a mistyped expression produces, is the real price of eliminating five objects down to one: a developer has to understand the lifetime rule to use the idiom safely, which is a cost a plain loop never asks anyone to pay.

## Self-registering prototypes: no central switch to edit

The second idiom in this chapter answers a different question: given a handful of concrete types known only by a string key, chosen from a config file or a command-line flag, how does code resolve that key into a fresh, independent instance without a function somewhere that lists every type by name? Chapter 13's `PackageTemplateRegistry` solves the cloning half of that problem, a `create()` method that calls `clone()` on a registered prototype, but it leaves registration itself as a manual step: some caller has to call `register_template("fragile-electronics", ...)` for every package kind, and a reviewer has to remember to touch that call site every time a new kind is added.

This chapter's `CarrierPolicyRegistry` solves registration the same way the clone half is already solved, through a method the concrete type itself owns rather than a method a caller has to remember:

```cpp
template <typename Concrete>
class CarrierPolicyRegistrar {
public:
    explicit CarrierPolicyRegistrar(std::string key) {
        CarrierPolicyRegistry::instance().register_prototype(std::move(key), std::make_unique<Concrete>());
    }
};
```

A concrete carrier policy, say `WeightTieredCarrierPolicy`, lives in its own translation unit and declares exactly one line at namespace scope:

```cpp
namespace {
class WeightTieredCarrierPolicy : public CarrierPolicy {
    // clone(), name(), base_rate_multiplier() implementations
};
const CarrierPolicyRegistrar<WeightTieredCarrierPolicy> registrar{"weight-tiered"};
}
```

That `registrar` object's constructor runs during static initialization, before `main()` starts, and nothing outside this file ever has to name `WeightTieredCarrierPolicy` for it to show up under the `"weight-tiered"` key later. Adding a fourth carrier means adding a fourth `.cpp` file with the same two-line pattern at the bottom; nothing in `carrier_policy.hpp`, in `CarrierPolicyRegistry`, or in any caller that resolves a key, changes at all. `tests/carrier_policy_registry_test.cpp` proves this is real rather than an accident of link order by resolving `"weight-tiered"` purely through the registry, with no `#include` anywhere in the test that names the concrete class.

This idiom has one sharp edge: it works in a quick local build and breaks once code ships as a library. A linker only pulls an object file out of a static archive when it can see a live symbol reference into that file. A self-registering type's only "reference" is the side effect of a global constructor nobody calls by name, which a linker can decide is unused and drop the entire translation unit, registrar and all, when that translation unit sits inside a `.a` archive. `examples/23-expression-templates/CMakeLists.txt` builds this chapter's core as an `OBJECT` library rather than `STATIC` so every `.cpp` file's object code passes straight through to the final link no matter what symbols it does or does not export; that is a build-system detail, not a C++ one, but it is the detail that decides whether this idiom works once it leaves a single translation unit.

## Templated serialization: one list drives both directions

The third idiom returns to a problem Chapter 13 already solved by hand: turning an object into text and back. `serialize_manifest()` and `deserialize_manifest()` there each write out one `std::format()` call, or one parse step, per field of `ShipmentManifest`, in two separate functions that a developer has to keep in sync by remembering to touch both whenever the manifest grows a field. This chapter's `CarrierProfile` solves the same problem with exactly one list instead of two functions written in parallel:

```cpp
constexpr auto describe_fields() {
    return std::make_tuple(field("carrier_name", &CarrierProfile::carrier_name),
                            field("base_rate_multiplier", &CarrierProfile::base_rate_multiplier),
                            field("max_weight_kg", &CarrierProfile::max_weight_kg));
}
```

`field()` pairs a human-readable name with a pointer to a data member, and `describe_fields()` is the single place every field of `CarrierProfile` is named. `serialize()` and `deserialize()` are both generic over that one tuple, walking it with `std::apply` and a fold expression rather than hand-writing one line per field:

```cpp
std::string serialize(const CarrierProfile& profile) {
    std::string text;
    std::apply(
        [&](const auto&... accessors) {
            ((text += std::string(accessors.name) + "=" + field_to_text(accessors.get(profile)) + "\n"), ...);
        },
        describe_fields());
    return text;
}
```

Adding a fourth field to `CarrierProfile` means adding one more `field(...)` call inside `describe_fields()`. Neither `serialize()` nor `deserialize()` has to change, because neither one knows how many fields exist or what they are called; both just walk whatever `describe_fields()` hands them. That is a stronger guarantee than discipline: Chapter 13's two hand-written functions can drift apart the moment one of them is edited and the other is forgotten, while this chapter's single list cannot drift from itself. `CarrierProfile` is also a plain copyable value, unlike Chapter 13's move-only `ShipmentManifest`, so its structural-equality check is a defaulted `operator==` rather than a hand-written field-by-field comparison function; `tests/carrier_profile_serialization_test.cpp` round-trips a profile through `serialize()` and `deserialize()` and checks the result against the original with that one defaulted operator.

## A narrower contrast: reporting why a lookup failed

Everything above compiles identically under C++20 and C++23. One corner of the registry idiom narrows to a single call site the way Chapter 8's error-handling contrast did: `CarrierPolicyRegistry::create()` resolves an unknown key to a null pointer, collapsing "the key was empty" and "the key was never registered" into the same uninformative result. A caller that wants to know which failure happened needs a richer return type.

{% include codetabs.html langs="C++20|C++23" %}
```cpp
// C++20: a null pointer is the only signal available. The caller learns
// *that* the lookup failed, never *why* it failed.
std::unique_ptr<CarrierPolicy> policy = registry.create(key);
if (!policy) {
    // was the key empty, or just unregistered? this line cannot say.
}
```
```cpp
// C++23: std::expected carries a typed reason alongside the success
// value, checkable with an if and no exception machinery.
std::expected<std::unique_ptr<CarrierPolicy>, std::string> result = registry.try_create(key);
if (!result.has_value()) {
    std::cout << "Rejected: " << result.error() << '\n';
}
```

`try_create()` is guarded behind `#if __cplusplus >= 202302L` in `src/carrier_policy.hpp` and `.cpp`, exactly as Chapter 8 and Chapter 13 guard their own C++23-only refinements, so the mandatory C++20 build path never requires `<expected>`. The registry itself, the self-registration mechanism, and every concrete carrier policy are identical in both standards; only this one lookup's failure reporting moved forward.

> **AI sidebar.** Ask an LLM pairing partner for an expression template and a common result skips the lifetime rule entirely: it returns `auto` from a helper function that builds a `RouteScaled` or a `RouteSum`, which looks like a tidy refactor and compiles cleanly, right up until a caller stores that `auto` and uses it on a later line after the reference it holds has gone stale. Read that result the way this chapter trained: an expression-template node's only legitimate home is inside the single full expression that builds a concrete result from it, never a named variable that outlives that expression. The same generated code often gets the self-registering registry wrong in a mirrored way, writing a `register_prototype()` call into a central `main()` or a factory function instead of into the concrete type's own translation unit, which reintroduces the central list this idiom exists to remove. Check whether the registration call lives inside the concrete type's own translation unit, with no caller anywhere naming the concrete class directly. If a reviewer has to open a central file to see every registered type, the generated code copied the shape of self-registration without its benefit.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan toolchain Chapter 0 installs; there is no container anywhere in this loop. From the repository root, the root superbuild picks up `examples/23-expression-templates` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's GoogleTest cases through `gtest_discover_tests`: 11 cases under the `cpp23` preset, 9 under `cpp20`, the difference being the two `std::expected`-specific cases that only compile under C++23. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 11   # cpp23
100% tests passed, 0 tests failed out of 9    # cpp20
```

To see the lazy and eager totals agree, and to see the construction counts that prove laziness rather than reading an assertion that says so, run the example's own demo script:

```sh
examples/23-expression-templates/demo.sh cpp23
```

It computes the same weighted three-leg route total both ways, lists the three self-registered carrier policies, and round-trips a carrier profile through text:

```
== Expression templates: weighted multi-leg route summary ==
Lazy weighted total: distance=121.2km fuel=18.15L dwell=0.85h
RouteMetrics objects constructed for the lazy expression: 1
Eager weighted total: distance=121.2km fuel=18.15L dwell=0.85h
NaiveRouteMetrics objects constructed for the eager expression: 5

== Self-registering prototype registry: carrier policies ==
flat-rate: base rate multiplier x1
weight-tiered: base rate multiplier x1.35
regional-express: base rate multiplier x1.8
3 carrier policies registered, none of them listed by hand in this file.

== C++23 refinement: typed lookup failure via std::expected ==
Rejected: carrier policy key must not be empty

== Templated serialization: round-tripping a carrier profile ==
carrier_name=Northline Freight
base_rate_multiplier=1.350000
max_weight_kg=18000.000000
Round trip reproduced an equal profile: yes
```

Notice the lazy and eager totals match exactly, distance, fuel, and dwell time all identical, while the construction counts differ by five to one. That gap is the entire argument for expression templates, visible at the terminal rather than asserted in prose. Running `examples/23-expression-templates/demo.sh cpp20` prints the same output minus the `std::expected` block, which only compiles under the `cpp23` preset.

## When not to use these idioms

Expression templates earn their complexity when the type being combined is expensive enough to copy or allocate that eliminating intermediates is worth measuring, and when the same combining expression runs often enough, in a hot loop or across a large batch, for that saving to matter. A `RouteMetrics` with three `double` fields sits right at the edge of that bar in real code; this chapter picked it because the mechanism is easiest to see against a small type, not because three doubles is where a production codebase should reach for expression templates first. The idiom pays for itself on something like a dense numeric vector or matrix type, the classic case `std::valarray` and every linear-algebra library built on expression templates targets, where a naive `a + b + c` would otherwise allocate and copy megabytes of intermediate storage per statement. Below that bar, a plain loop or a direct eager computation costs less to write, compiles faster, steps cleanly through a plain debugger without five layers of template instantiation in the way, and gives a compiler error that names a concrete type instead of a wall of substitution failures three templates deep. If nobody can point at a profiler result or a concrete allocation count that an expression template would fix, write the eager version and keep the option open for later.

The self-registering registry and the templated serialization hook both earn their cost under a similar test: a closed, growing set of kinds that all need the same treatment, registered or serialized through the same mechanism. A registry with one concrete type registered, or a `describe_fields()` list that two different call sites never share, is paying the complexity of genericity for no benefit a single concrete function would not give more directly. The same engineering question Chapter 1 trained runs through every idiom in this chapter: name the plain alternative, a direct function call, a hand-written `if`/`else if`, two ordinary functions, next to the generic mechanism, and let the evidence of real duplication or a measured cost decide between them, not the possibility that modern C++ allows for a generic version.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): Prototype's original entry, the pattern-level foundation this chapter's self-registering registry builds on as an idiom, and the shared intent Chapter 13 already covers in more depth.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/23-expression-templates`'s suite, which asserts a construction count and a registry resolution across translation units rather than only checking that the binaries exit zero.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.

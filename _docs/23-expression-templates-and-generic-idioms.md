---
title: "Expression templates and generic idioms"
order: 23
part: "idiomatic-cpp-expressions"
description: "Expression templates that build a compile-time expression tree so a+b+c evaluates in one pass with no intermediate objects, plus two supporting idioms from the same freight-route domain: a self-registering Prototype registry and a single describe hook that drives both serialize() and deserialize()."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Explain why `a + b + c` over ordinary operator overloads builds one throwaway object per operator, and how an expression template defers evaluation to the final assignment.
- Build an expression template from a CRTP base, a sum node, a scaled node and one converting constructor.
- Keep expression nodes inside a single full expression so their references never dangle.
- Build a Prototype registry that concrete types join from their own translation units.
- Drive `serialize()` and `deserialize()` from one field list so they can't drift apart.
- Tell when a plain loop or two plain functions are the better choice.

## Why this matters

A freight dispatcher closing out a route report needs a weighted combination of three legs' metrics: distance, fuel burned and dwell time at each handoff dock. Written against operators that each return a new object by value, every `+` and `*` in `0.5 * leg_one + 0.3 * leg_two + 0.2 * leg_three` builds an intermediate that exists only to feed the next operator: five throwaway objects for one answer.

An expression template makes each operator return a small object that describes the arithmetic. Nothing runs until the chain is assigned to a real variable, and then it runs once, field by field, and builds one object. Two related idioms work the same freight domain: Chapter 13's hand-registered `PackageTemplateRegistry` becomes a registry that types join themselves, and Chapter 13's hand-written `serialize_manifest()`/`deserialize_manifest()` pair becomes two generic functions driven by one field list.

{% include excalidraw.html name="23-expression-templates" caption="Figure 23.1: an expression template defers 0.5 x leg_one + 0.3 x leg_two + 0.2 x leg_three into a tree of RouteScaled/RouteSum nodes that evaluates once into a RouteMetrics, against the eager version that builds a new NaiveRouteMetrics object at every operator." %}

## Pattern: Expression template

### The problem

`RouteMetrics` carries three measurements for one leg of a route:

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

Eager operators on it construct a full `RouteMetrics` at every step of a weighted sum. On a large numeric type, each of those intermediates is an allocation and a copy.

### The solution

An **expression template** makes each operator return a lightweight node that records the operation, so the whole expression is evaluated in a single pass when it's assigned to a concrete result.

### How C++ models it

`RouteExpr<Derived>` is a CRTP base, the shape [CRTP and policy-based design]({{ '/docs/21-crtp-and-policy-based-design/' | relative_url }}) introduced, and it supplies one thing: a typed cast back to the concrete node. Two node types hold the deferred arithmetic:

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

Neither computes anything in its constructor. Each holds references to its operands and does its add or multiply in `operator[]`, once per field, when asked. The operators build nodes instead of a `RouteMetrics`:

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

The three-leg sum becomes a typed tree of five nodes: three `RouteScaled`, then two `RouteSum`. Each is a reference and perhaps a `double` on the stack. The only place arithmetic runs, and the only place a `RouteMetrics` is built, is the converting constructor:

```cpp
template <typename Expr>
RouteMetrics(const RouteExpr<Expr>& expr)
    : values_{expr.derived()[0], expr.derived()[1], expr.derived()[2]} {
    ++g_construction_count;
}
```

It asks the root for each field, and the tree asks its operands down to the three original legs, all inside this one call.

### Counting the temporaries

A shared construction counter and an eager comparison type, `NaiveRouteMetrics`, whose operators each return a new object, make the cost countable. The same weighted sum gives equal results both ways:

| Path | Objects constructed |
| --- | --- |
| Expression template, `RouteMetrics` | 1 |
| Eager operators, `NaiveRouteMetrics` | 5 (three multiplies, two adds) |

### Keep nodes inside one statement

Nodes hold references, which is what keeps them cheap, and those references are valid only until the end of the full expression that built them. `auto blend = 0.5 * leg_one + 0.3 * leg_two;` stores a `RouteSum` that refers to two `RouteScaled` temporaries, and both die at the semicolon. Using `blend` on the next line reads dangling references, and the type system doesn't flag it. Assign every expression to a concrete `RouteMetrics` in the statement that builds it, and never name the node type. That rule, and the template error messages a mistyped expression produces, are what the idiom costs.

## Pattern: Self-registering prototype registry

### The problem

Carrier policies are chosen by a string key from a config file or a command-line flag. Chapter 13's `PackageTemplateRegistry` clones a registered prototype, but some caller must call `register_template()` for every kind, and each new kind means editing that call site.

### The solution

A **self-registering registry** has each concrete type register its own prototype from its own translation unit during static initialization, so no central list names the concrete types.

### How C++ models it

{% include excalidraw.html name="23-prototype-registry" caption="Figure 23.2: each carrier policy's translation unit registers its own prototype at static init, so a caller resolves a key through CarrierPolicyRegistry and gets back a fresh clone without any file naming the concrete type." %}

A registrar template registers a default-constructed prototype in its constructor:

```cpp
template <typename Concrete>
class CarrierPolicyRegistrar {
public:
    explicit CarrierPolicyRegistrar(std::string key) {
        CarrierPolicyRegistry::instance().register_prototype(std::move(key), std::make_unique<Concrete>());
    }
};
```

Each concrete policy lives in its own `.cpp` file and declares one registrar at namespace scope:

```cpp
namespace {
class WeightTieredCarrierPolicy : public CarrierPolicy {
    // clone(), name(), base_rate_multiplier() implementations
};
const CarrierPolicyRegistrar<WeightTieredCarrierPolicy> registrar{"weight-tiered"};
}
```

The registrar's constructor runs before `main()`, and no file outside this one names `WeightTieredCarrierPolicy`. A fourth carrier is a fourth `.cpp` file; `carrier_policy.hpp`, `CarrierPolicyRegistry` and every caller stay unchanged. Callers resolve `"weight-tiered"` through the registry alone.

### Static archives drop registrars

A linker pulls an object file out of a static archive only when something references a symbol in it. A self-registering file's only effect is a global constructor nobody calls, so a `.a` archive can drop the whole file, registrar and all. `examples/23-expression-templates/CMakeLists.txt` builds the core as an `OBJECT` library rather than `STATIC`, so every object file reaches the final link.

### A typed lookup failure

`CarrierPolicyRegistry::create()` returns a null pointer for an unknown key, so "the key was empty" and "the key was never registered" look the same. `std::expected`, new in C++23, carries the reason. `try_create()` sits behind `#if __cplusplus >= 202302L` in `src/carrier_policy.hpp` and `.cpp`, so the C++20 build never needs `<expected>`; everything else is identical in both standards.

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

## Pattern: Describe-driven serialization

### The problem

Chapter 13's `serialize_manifest()` and `deserialize_manifest()` each spell out every field of `ShipmentManifest`. When the manifest grows a field, both must change, and nothing catches an edit to one without the other.

### The solution

**Describe-driven serialization** names each field once, in a single describe hook, and writes `serialize()` and `deserialize()` generically over that list.

### How C++ models it

`CarrierProfile` lists its fields in one place:

```cpp
constexpr auto describe_fields() {
    return std::make_tuple(field("carrier_name", &CarrierProfile::carrier_name),
                            field("base_rate_multiplier", &CarrierProfile::base_rate_multiplier),
                            field("max_weight_kg", &CarrierProfile::max_weight_kg));
}
```

`field()` pairs a name with a pointer to a data member. Both directions walk the tuple with `std::apply` and a fold expression:

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

A fourth field is one more `field(...)` call; neither `serialize()` nor `deserialize()` changes, because neither knows how many fields exist. `CarrierProfile` is a plain copyable value, unlike Chapter 13's move-only `ShipmentManifest`, so a defaulted `operator==` checks a round trip.

<blockquote class="ai-sidebar" markdown="1">
**Specifying an expression template.** "Make the weighted route sum avoid temporaries" doesn't say where the nodes may live or where registration goes, and a helper that returns `auto` from a `RouteSum` compiles cleanly and dangles later.

> In `examples/23-expression-templates/src/route_metrics.hpp`, add a `RouteDifference<Left, Right>` node derived from `RouteExpr`, holding `const` references like `RouteSum`, and an `operator-` over `RouteExpr<Left>` and `RouteExpr<Right>`. Evaluate only in `RouteMetrics`'s converting constructor. Don't add any function that returns a node type, and don't store a node in a named variable. Add a fourth carrier policy as a new `.cpp` file with its own `CarrierPolicyRegistrar` at namespace scope, and don't edit `carrier_policy.hpp`, `carrier_policy.cpp` or `demo_main.cpp`.

Check: `ctest --preset cpp23 -R 'ExpressionTemplateTest|CarrierPolicyRegistryTest'` passes, and `grep -n 'auto' src/route_metrics.hpp` shows no function returning a node.
</blockquote>

## When not to use it

- **Expression templates.** They pay on large numeric types, the dense vectors and matrices that `std::valarray` and linear-algebra libraries target, where an eager `a + b + c` copies megabytes per statement, inside a hot loop. Three `double` fields sit at the edge of that bar; `RouteMetrics` is small so the mechanism is easy to see. Below the bar, an eager computation compiles faster, steps through a debugger without layers of instantiation and gives errors that name a concrete type. Without a profile or an allocation count that the template fixes, write the eager version.
- **Self-registering registry.** With one registered type, or a key set fixed in one place, a direct call or a plain `if`/`else if` is clearer.
- **Describe hook.** When only one function walks the fields, two hand-written functions, or one, cost less than the generic machinery.

## Build, run, observe

From the repository root, the superbuild picks up `examples/23-expression-templates`:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All 11 cases pass under `cpp23` and 9 under `cpp20`; the two `std::expected` cases compile only under C++23. The demo computes the weighted three-leg total both ways, lists the self-registered carrier policies and round-trips a carrier profile:

```sh
examples/23-expression-templates/demo.sh cpp23
```

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

The lazy and eager totals match, and the construction counts are 1 and 5. Under `cpp20` the `std::expected` block is absent.

One experiment shows why the core is an `OBJECT` library:

- **Archive the core.** In a scratch copy, change `add_library(exprtmpl23_core OBJECT` to `STATIC` in `CMakeLists.txt`. The build succeeds, the linker drops the three policy files, and 3 of the 11 cases fail because the registry is empty.

## For deeper coverage

- Erich Gamma, Richard Helm, Ralph Johnson, and John Vlissides, [*Design Patterns: Elements of Reusable Object-Oriented Software*](https://www.pearson.com/en-us/subject-catalog/p/design-patterns-elements-of-reusable-object-oriented-software/P200000009480) (Addison-Wesley): Prototype's original entry, the foundation the self-registering registry builds on.
- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom behind `examples/23-expression-templates`.
- The full reading list lives on the [bibliography page]({{ '/bibliography/' | relative_url }}).

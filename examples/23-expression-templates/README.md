# 23 - Expression templates and two supporting idioms

A freight dispatcher summarizing a multi-leg route needs a weighted
combination of several legs' metrics, for instance a distance-weighted
total across three legs of a shipment's journey, computed once with no
scratch objects left behind. This example builds that weighted sum as an
expression template, then adds two supporting C++ idioms from the same
codebase: a self-registering Prototype registry and a templated
serialization format.

## The domain

- `RouteMetrics` (`src/route_metrics.hpp`): three physical metrics per
  route leg, `distance_km`, `fuel_liters`, `dwell_hours`, all `double`.
- `CarrierPolicy` (`src/carrier_policy.hpp`): a cloneable carrier pricing
  policy, selected by name from a registry.
- `CarrierProfile` (`src/carrier_profile_serialization.hpp`): a plain
  value struct describing a carrier contract, round-tripped through text.

## Idiom one: expression templates

`RouteMetrics` derives from the CRTP base `RouteExpr<RouteMetrics>`, and
`operator+`/`operator*` build lightweight `RouteSum`/`RouteScaled` node
types instead of a new `RouteMetrics` at every step. A chain like
`0.5 * leg_one + 0.3 * leg_two + 0.2 * leg_three` builds a small tree of
those nodes and evaluates every component exactly once, when the result is
finally assigned to a `RouteMetrics`, through the class's templated
converting constructor, the only place in this example a `RouteMetrics`
gets built from an expression.

`src/eager_route_metrics.hpp` defines `NaiveRouteMetrics`, the same fields
and the same math, but with ordinary `operator+`/`operator*` that each
return a brand-new value immediately. Both types share a construction
counter (`src/construction_tracking.hpp`) that the test suite resets and
reads to prove the expression-template path constructs exactly one result
object for a three-term weighted sum, while the eager path constructs five
(three scaled values, two sums), and that both compute identical results.

## Idiom two: self-registering prototype registry

`CarrierPolicy` (`src/carrier_policy.hpp`) is a cloneable base; concrete
policies (`src/flat_rate_carrier_policy.cpp`,
`src/weight_tiered_carrier_policy.cpp`,
`src/regional_express_carrier_policy.cpp`) each live in their own
translation unit, in an anonymous namespace, and register themselves into
`CarrierPolicyRegistry` through a `CarrierPolicyRegistrar<Concrete>`
instance declared at namespace scope. That registrar's constructor runs
during static initialization, before `main()`, so adding a fourth carrier
means adding one new `.cpp` file, not editing a central switch or list.

`CMakeLists.txt` builds this example's core as an `OBJECT` library rather
than `STATIC`, specifically so the linker cannot drop a self-registering
translation unit that nothing else references by symbol.

## Idiom three: templated serialization

`src/carrier_profile_serialization.hpp` defines one `describe_fields()`
function returning a `std::tuple` of `(name, member pointer)` pairs for
`CarrierProfile`. `serialize()` and `deserialize()` are both generic over
that one list via `std::apply` and a fold expression; adding a field to
`CarrierProfile` means editing `describe_fields()` once, not editing a
hand-written `serialize()` and a hand-written `deserialize()` separately.

### C++23 refinement (guarded)

`CarrierPolicyRegistry::try_create()` returns
`std::expected<std::unique_ptr<CarrierPolicy>, std::string>` instead of
collapsing "empty key" and "unknown key" into the same null result. It is
compiled only under C++23 (`#if __cplusplus >= 202302L` in
`src/carrier_policy.hpp` and `.cpp`), so the mandatory C++20 build path
never needs `<expected>`. The matching gtest cases in
`tests/carrier_policy_registry_test.cpp` are guarded the same way.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/23-expression-templates
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

Either path discovers the same 11 gtest cases under `cpp23` (9 under
`cpp20`: the two `std::expected` cases are compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 11   # cpp23
100% tests passed, 0 tests failed out of 9    # cpp20
```

To see the expression-template weighted sum, the self-registering
registry, and the serialization round trip run end to end, build and run
the demo binary. From this directory, `./demo.sh` does the same steps
above plus runs it; or build just the demo target after a
`cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target exprtmpl23_demo
./build/cpp23/exprtmpl23_demo
```

Expected output:

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

The lazy and eager totals agree exactly; only the construction count
differs, by five to one. Running `examples/23-expression-templates/demo.sh
cpp20` prints the same output minus the `std::expected` block, which only
compiles under the `cpp23` preset.

## Files

```
examples/23-expression-templates/
  CMakeLists.txt            # standalone + superbuild-integrated targets (OBJECT library)
  CMakePresets.json          # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py                # standalone Conan recipe (gtest/1.17.0)
  demo.sh                      # build + run the demo binary
  src/
    construction_tracking.hpp     # shared constructor counter used by tests
    route_metrics.hpp/.cpp        # expression templates: RouteExpr/RouteSum/RouteScaled/RouteMetrics
    eager_route_metrics.hpp/.cpp  # NaiveRouteMetrics, the eager baseline
    carrier_policy.hpp/.cpp       # CarrierPolicy + CarrierPolicyRegistry + CarrierPolicyRegistrar
    flat_rate_carrier_policy.cpp         # self-registering concrete policy
    weight_tiered_carrier_policy.cpp     # self-registering concrete policy
    regional_express_carrier_policy.cpp  # self-registering concrete policy
    carrier_profile_serialization.hpp/.cpp  # describe_fields() + serialize()/deserialize()
    demo_main.cpp                 # exercises all three idioms
  tests/
    expression_template_test.cpp
    carrier_policy_registry_test.cpp
    carrier_profile_serialization_test.cpp
```

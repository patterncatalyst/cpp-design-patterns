# 17 - Composite, Flyweight, and Null Object: palletizing a shipment

A pallet is built from cartons, and a carton can itself hold more cartons
before it holds any items. Whatever level you query, from one item up to
the whole pallet, the question is the same: how much does this weigh, and
how many units does it hold. That is Composite. The items on that pallet
describe a handful of SKUs over and over, by the hundreds; a Flyweight
pool shares one immutable descriptor per SKU instead of copying it into
every item that references it. And a weight-limit check needs somewhere to
report an overweight carton, whether or not the caller supplied a real
handler; a Null Object stands in by default so that call site never branches
on "is there a handler."

## The domain

- `SkuDescriptor` (`src/sku_catalog.hpp`): immutable intrinsic state for one
  SKU (`sku_code`, `product_name`, `unit_weight_grams`, dimensions,
  `hazmat_class`), shared by every `Item` that references it.
- `Item` (`src/package_tree.hpp`): a leaf `PackageComponent` holding a shared
  `SkuDescriptor` pointer plus its own extrinsic `quantity` and
  `destination`.
- `PackageGroup`: a composite `PackageComponent` that is a carton or a
  pallet depending only on what you put in it; nesting one `PackageGroup`
  inside another is how a mixed tree (pallet -> cartons -> items, with
  cartons nested inside cartons) gets built.
- `ShipmentExceptionHandler` (`src/exception_handler.hpp`): the interface a
  `PackageGroup` reports an overweight condition to.

## The three idioms

1. **Composite** (`src/package_tree.hpp`): `PackageComponent` declares
   `total_weight_grams()` and `item_count()`; `Item` (leaf) and
   `PackageGroup` (composite) both implement it, so a caller holding a
   `PackageComponent` never needs to know whether it has one item or an
   entire pallet. `PackageGroup::total_weight_grams()` and `item_count()`
   both recurse by summing whatever their children report, regardless of
   whether a child is itself a leaf or another `PackageGroup`.
2. **Flyweight** (`src/sku_catalog.hpp`): `SkuCatalog` separates a
   registered product catalog (every SKU a warehouse could ship) from the
   flyweight pool itself (only the SKUs some caller has actually requested,
   materialized once each). `get()` returns the same
   `std::shared_ptr<const SkuDescriptor>` for the same SKU code every time,
   and `pool_size()` tracks the number of distinct SKUs materialized, not
   the number of `Item`s referencing them.
3. **Null Object** (`src/exception_handler.hpp`): `NullShipmentExceptionHandler`
   is a do-nothing `ShipmentExceptionHandler`. `PackageGroup` binds a
   `const ShipmentExceptionHandler&` at construction, defaulting to
   `null_shipment_exception_handler()`, so `check_weight_limit()` calls
   `handler_.on_overweight(...)` unconditionally: the call site never has an
   `if (handler_)` branch anywhere.

### C++23 refinement (guarded)

`SkuCatalog`'s pool storage is a `std::map` under C++20 and a `std::flat_map`
under C++23 (`#if __cplusplus >= 202302L` in `src/sku_catalog.hpp`): the
flat, vector-backed container is friendlier to the cache for the small pool
a pallet's worth of SKUs produces, at the cost of iterator/reference
stability across an insertion, which this example's call sites never rely
on. `SkuCatalog::try_get()` additionally returns
`std::expected<std::shared_ptr<const SkuDescriptor>, std::string>` instead
of throwing, compiled only under C++23. The matching gtest cases in
`tests/sku_catalog_test.cpp` are guarded the same way.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/17-composite-flyweight
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

Either path discovers the same 18 gtest cases under `cpp23` (16 under
`cpp20`: the two `std::expected`-specific cases are compiled out) via
`gtest_discover_tests`. Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 18   # cpp23
100% tests passed, 0 tests failed out of 16   # cpp20
```

To see the three idioms at work rather than just reading an assertion that
says they work, build and run the demo binary. From this directory,
`./demo.sh` does the same three steps above plus runs it; or build just the
demo target after a `cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target comp17_demo
./build/cpp23/comp17_demo
```

Expected output:

```
== Composite: a mixed tree, one interface ==
Carton CTN-A: 14800g, 52 units
Carton CTN-C: 7200g, 8 units
Carton CTN-B: 7200g, 8 units
Pallet PAL-9: 22000g, 60 units
(Pallet's totals are Carton CTN-A's and Carton CTN-B's totals added together,
 and Carton CTN-B's total is Carton CTN-C's total, recursed through two levels.)

== Flyweight: a few descriptors, many line items ==
Distinct SKUs registered in the catalog: 3
Flyweights materialized so far: 3
After adding 60 more line items across the same 3 SKUs:
Flyweights materialized: 3 (still 3)
Two separate get("WIDGET-100") calls return the same instance: true

== Null Object: a weight-limit check with no branch for "no handler" ==
Carton CTN-A (default, unhandled) checked against a 500g limit:
  (no output above: the default NullShipmentExceptionHandler ran and did nothing)
  Carton CTN-A (logged) is 14300g over its weight limit

== C++23 refinement: typed error for an unregistered SKU ==
Rejected lookup: unregistered SKU code: CRATE-999
```

The last block only prints under the `cpp23` preset; under `cpp20` the demo
stops after the Null Object section.

## Files

```
examples/17-composite-flyweight/
  CMakeLists.txt        # standalone + superbuild-integrated targets
  CMakePresets.json       # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py             # standalone Conan recipe (gtest/1.17.0)
  demo.sh                  # build + run the demo binary
  src/
    sku_catalog.hpp/.cpp        # Flyweight: SkuDescriptor + SkuCatalog
    exception_handler.hpp/.cpp  # Null Object: ShipmentExceptionHandler family
    package_tree.hpp/.cpp       # Composite: PackageComponent, Item, PackageGroup
    demo_main.cpp                 # exercises all three
  tests/
    sku_catalog_test.cpp
    exception_handler_test.cpp
    package_tree_test.cpp
```

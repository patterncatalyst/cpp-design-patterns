# 09 - Template Method and the non-virtual interface: order export pipeline

A warehouse system has to hand an order off to three different downstream
systems, one wanting a CSV manifest, one wanting newline-delimited JSON, one
wanting a fixed-width EDI feed, and the sequence of steps that produces any
one of those documents never changes: validate the order, write a header,
write one line per item, write a footer, append a trailer. Only the content
of three of those steps differs between formats. This example expresses
that shape with Template Method plus the Non-Virtual Interface (NVI)
idiom: a public, non-virtual method owns the fixed sequence, and private
virtual hooks supply the parts that vary.

## The domain

- `Order`: an `order_id`, a `customer_name`, and a list of `OrderLine`
  values (`sku`, `quantity`).
- `total_quantity()` in `src/order_domain.cpp` is the one place an order's
  units get summed, shared by every exporter's trailer so the reported
  count can never drift from the order's actual lines.

## The skeleton and its hooks

`OrderExportPipeline::export_order()` (`src/export_pipeline.hpp`) is the
one public entry point and it is never virtual:

1. `validate()` (private, non-virtual, static): rejects an order with no
   line items. Not a hook; every concrete exporter gets this check for
   free and none can skip it.
2. `render_header()` (private virtual hook)
3. `render_line()` (private virtual hook, called once per `OrderLine`)
4. `render_footer()` (private virtual hook)
5. `trailer_for()` (private, non-virtual): appends a trailer line naming
   the format and the total unit count. Not a hook either; it always runs
   after `render_footer()`, regardless of what `render_footer()` produced.

Three concrete exporters override the three hooks:

- **`CsvManifestExporter`**: a two-line header naming the order, then one
  comma-separated row per line item.
- **`JsonManifestExporter`**: newline-delimited JSON, one object per line,
  no wrapping array and no comma bookkeeping between elements.
- **`FixedWidthEdiExporter`**: fixed-width, space- or zero-padded
  segments (`ORD`, `LIN`, `END`), the shape a legacy trading-partner feed
  still expects.

Hooks are declared `private`, not `protected`: a subclass supplies them
but has no way to call them itself, and nothing outside `OrderExportPipeline`
can reach them at all. That is NVI's second half, alongside the public
method being non-virtual.

## Static dispatch variant: CRTP versus deducing this

`src/static_dispatch_variant.hpp` is a narrower, separate illustration:
one hook, `describe()`, dispatched at compile time instead of through a
vtable.

- **C++20**: `CrtpSummarySkeleton<Derived>` is the Curiously Recurring
  Template Pattern. The base class itself must be a template
  parameterized on the derived type, and `summarize()` casts `*this` down
  to `Derived` explicitly.
- **C++23** (guarded, `#if __cplusplus >= 202302L`): `DeducingThisSummarySkeleton`
  uses an explicit object parameter (`this const Self& self`) instead.
  The base class is an ordinary, non-template class; the compiler deduces
  `Self` as the most-derived type at the call site, with no CRTP template
  argument and no `static_cast` anywhere.

Neither of these two classes replaces `OrderExportPipeline`; they exist to
show what changes, and what does not, when a Template Method's hook moves
from runtime to compile-time dispatch.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/09-template-method
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

Either path discovers the same gtest cases via `gtest_discover_tests`: 10
under `cpp23`, 8 under `cpp20` (the two deducing-this cases are compiled
out). Observed locally on Fedora 44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 10   # cpp23
100% tests passed, 0 tests failed out of 8    # cpp20
```

To see all three formats export the same order side by side, build and
run the demo binary. From this directory, `./demo.sh` does the same steps
above plus runs it; or build just the demo target after a
`cmake --build --preset cpp23`:

```sh
cmake --build --preset cpp23 --target tmpl9_demo
./build/cpp23/tmpl9_demo
```

Expected output:

```
== CSV manifest (CsvManifestExporter) ==
order_id,customer_name
ORD-77,Riverside Hardware Co.
sku,quantity
SKU-100,3
SKU-205,1
-- 4 unit(s) exported via CSV --

== Newline-delimited JSON (JsonManifestExporter) ==
{"order_id":"ORD-77","customer_name":"Riverside Hardware Co."}
{"sku":"SKU-100","quantity":3}
{"sku":"SKU-205","quantity":1}
-- 4 unit(s) exported via JSON --

== Fixed-width EDI (FixedWidthEdiExporter) ==
ORDORD-77      Riverside Hardware Co.  
LINSKU-100     00003
LINSKU-205     00001
END00002
-- 4 unit(s) exported via fixed-width EDI --

== Static dispatch: CRTP (C++20) ==
ORD-77: 2 line(s)

== Static dispatch: deducing this (C++23) ==
ORD-77: 2 line(s)
```

The last block only prints under the `cpp23` preset; under `cpp20` the
demo stops after the CRTP section.

## Files

```
examples/09-template-method/
  CMakeLists.txt        # standalone + superbuild-integrated targets (tmpl9_ prefix)
  CMakePresets.json      # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py            # standalone Conan recipe (gtest/1.17.0)
  demo.sh                 # build + run the demo binary
  src/
    order_domain.hpp/.cpp         # Order, OrderLine, total_quantity()
    export_pipeline.hpp/.cpp      # OrderExportPipeline (NVI) + 3 concrete exporters
    static_dispatch_variant.hpp   # CRTP (C++20) vs deducing this (C++23)
    demo_main.cpp                  # exercises all three formats + both static variants
  tests/
    export_pipeline_test.cpp
    static_dispatch_variant_test.cpp
```

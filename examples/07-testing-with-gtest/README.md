# 07 - Proving it works: testing with GoogleTest

A freight carrier's quote intake has to validate a shipment request, price
it, and record the result through a ledger before it is final. This
example builds that component, `QuoteIntake`, and proves it with the full
range of GoogleTest and GoogleMock styles this chapter teaches, each one
used where it fits a real need in the suite rather than for completeness.

## The domain

- `ShipmentRequest`: `weight_kg`, a `ServiceZone` (`Local`, `Regional`,
  `National`, `International`, `Unassigned`), and `declared_value_cents`.
- `validate_shipment()` rejects a non-positive weight, an unresolved zone,
  or a declared value over the carrier's insurable limit; everything else
  is `Accepted`.
- `quote_cost()` prices an accepted request. Money is tracked as integer
  `Cents`, never floating point, so a price is exact and reproducible in a
  test assertion.
- `QuoteIntake` ties the two together and depends on two collaborators
  through interfaces it never implements itself: `QuoteIdGenerator` and
  `QuoteLedger`.

## The five test styles

1. **Parameterized test** (`tests/quote_domain_test.cpp`): `TEST_P` +
   `INSTANTIATE_TEST_SUITE_P` over a table of `ShipmentRequest` ->
   `ValidationOutcome` cases, each one named so `ctest -N` reads like a
   requirements list instead of a row number.
2. **Death test** (`tests/quote_domain_test.cpp`): `EXPECT_DEATH` proves
   `quote_cost()`'s precondition: calling it with a negative weight aborts
   the process. The precondition is checked by `FREIGHT_REQUIRE`
   (`src/quote_domain.hpp`), not the standard `assert()`, because this
   book's CMake presets build everything as Release, which defines
   `NDEBUG` and compiles `assert()` down to nothing.
3. **Typed test** (`tests/quote_id_generator_test.cpp`): `TYPED_TEST_SUITE`
   runs the same two contract checks (never empty, never repeats) against
   both `SequentialQuoteIdGenerator` and `PrefixedQuoteIdGenerator` without
   writing the loop twice.
4. **Fixture with `SetUp`/`TearDown`** (`tests/quote_intake_test.cpp`):
   `QuoteIntakeTest` resets `SequentialQuoteIdGenerator`'s process-wide
   counter before and after every test, because that counter is shared
   state a fresh fixture member does not reset on its own.
5. **GoogleMock interaction test** (`tests/quote_intake_test.cpp`):
   `MockQuoteLedger` with `EXPECT_CALL`, the `Field` matcher, and the
   `Return` action proves `QuoteIntake` calls its ledger with the right
   argument, the right number of times, and defers to what the ledger
   reports back.

`tests/quote_intake_test.cpp` also contrasts a fatal `ASSERT_TRUE` (where
a `false` result would make the next line undefined behavior) against the
non-fatal `EXPECT_EQ` checks that follow it, each with a failure message
that says what probably broke.

## Build, run, observe

No containers: local clang + CMake + Ninja + Conan only (see the
prerequisites chapter and `scripts/check-prereqs.sh` at the repo root).

This example is standalone-buildable from this directory:

```sh
cd examples/07-testing-with-gtest
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23
cmake --build --preset cpp23
ctest --preset cpp23 --output-on-failure
```

It is also picked up by the repo root's superbuild. From the repo root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

Either path discovers the same 15 gtest cases under both `cpp23` and
`cpp20` via `gtest_discover_tests`; this example has no C++23-only code,
so the count does not change between presets. Observed locally on Fedora
44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 15   # cpp23
100% tests passed, 0 tests failed out of 15   # cpp20
```

To see intake validate, price, and record a shipment rather than just
reading an assertion that says it works, build and run the demo binary.
From this directory, `./demo.sh` does the same three steps above plus
runs it; or build just the demo target after a `cmake --build --preset
cpp23`:

```sh
cmake --build --preset cpp23 --target testing7_quote_demo
./build/cpp23/testing7_quote_demo
```

Expected output:

```
== Validation: checking five requests before any of them is priced ==
  weight=3.5kg -> accepted
  weight=0kg -> rejected: weight_kg must be positive
  weight=-2kg -> rejected: weight_kg must be positive
  weight=1kg -> rejected: destination zone is not set
  weight=1kg -> rejected: declared value exceeds the insurable limit

== Intake: validating, pricing, and recording through the ledger ==
  Q-0001: $12.25 (Regional, 3.5kg)
  Q-0002: $32.00 (International, 8kg)
  Rejected before pricing: rejected: weight_kg must be positive

== Ledger decline: the ledger, not validation, says no ==
  Ledger declined to record the quote: quote_id already on file

Ledger now holds 3 quotes.
```

Running `examples/07-testing-with-gtest/demo.sh cpp20` prints the same
output; nothing in this example's demo path depends on the standard.

## Files

```
examples/07-testing-with-gtest/
  CMakeLists.txt        # standalone + superbuild-integrated targets
  CMakePresets.json      # cpp23 / cpp20 presets, mirrors the repo root
  conanfile.py            # standalone Conan recipe (gtest/1.17.0, gmock included)
  demo.sh                 # build + run the demo binary
  src/
    quote_domain.hpp/.cpp        # ShipmentRequest, validate_shipment, quote_cost, FREIGHT_REQUIRE
    quote_ledger.hpp              # Quote, QuoteLedger interface
    quote_id_generator.hpp/.cpp  # QuoteIdGenerator interface + two implementations
    quote_intake.hpp/.cpp         # QuoteIntake: the component under test
    demo_main.cpp                  # exercises validation, intake, and a ledger decline
  tests/
    mock_quote_ledger.hpp          # GoogleMock MockQuoteLedger
    quote_domain_test.cpp          # parameterized test + death test
    quote_id_generator_test.cpp    # typed test
    quote_intake_test.cpp          # fixture + GoogleMock interaction tests
```

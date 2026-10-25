---
title: "Testing with GoogleTest"
order: 34
layout: tutorial
sectionid: docs
part: appendices
appendix: "B"
description: "A reference for the GoogleTest macros, assertions, fixtures, and parameterized/typed/death tests this book's examples use, and how they run through CTest."
duration: "15 min"
---

## What this appendix covers

Every example's test suite in this book is built on GoogleTest. The testing
chapter builds that foundation narratively, against a freight carrier's quote
intake. This appendix is the lookup table: the macros, what each one expands
to or guarantees, and when to reach for which one. Mocking with GoogleMock has
its own appendix; nothing here covers `MOCK_METHOD` or `EXPECT_CALL` beyond
this one line.

## TEST, TEST_F, and TEST_P: three ways to register a case

`TEST(Suite, Name)` is the plain form. It expands to a class derived from
`::testing::Test`, named from the suite and test name, with the macro's body
poured into an overridden `TestBody()` method. A file-scope static object
registers that class with GoogleTest's global test registry before `main()`
runs, which is how `ctest -N` can list every case without executing any of
them: discovery walks the registry, not the test bodies.

```cpp
TEST(RateCardTest, RegionalZoneUsesTheBaseRate) {
    RateCard card;
    EXPECT_EQ(card.rate_for_zone(ServiceZone::Regional), Cents{480});
}
```

`TEST_F(Fixture, Name)` is the same expansion, except the generated class
derives from `Fixture` instead of `::testing::Test` directly, so the test body
runs with that fixture's members and its `SetUp()`/`TearDown()` already wired
in. `TEST_P(Fixture, Name)` goes one step further: `Fixture` must derive from
`::testing::TestWithParam<T>`, and the generated `TestBody()` calls
`GetParam()` to read whichever value `INSTANTIATE_TEST_SUITE_P` is currently
feeding it. All three macros produce a class and register it; they differ
only in what that class inherits and how many instances the registry ends up
holding.

## Assertions: EXPECT_* versus ASSERT_*

GoogleTest's two assertion families differ in exactly one respect: what
happens immediately after one of them fails. `EXPECT_*` records the failure
and lets the rest of the test function keep running. `ASSERT_*` records the
failure and returns from the enclosing function on the spot, skipping
everything after it, but it does not end the program or the test binary;
`TearDown()` and every other test in the suite still run.

Use `ASSERT_*` where running the next line after a failure would be undefined
behavior or meaningless, not merely undesirable:

```cpp
TEST_F(ShipmentTrackerTest, RecordsAndReturnsTheStoredShipment) {
    tracker_.record(Shipment{"TRK-900", 4.2, carrier_id_});

    const auto found = tracker_.find("TRK-900");

    ASSERT_TRUE(found.has_value()) << "a shipment just recorded must be findable by its tracking id";
    EXPECT_EQ(found->weight_kg, 4.2);
    EXPECT_EQ(found->carrier, carrier_id_);
}
```

`found` is a `std::optional<Shipment>`. Dereferencing it after a failed
`has_value()` check would be undefined behavior, so that check has to be
`ASSERT_TRUE`. The two `EXPECT_EQ` checks after it are independent facts
about the same shipment; both deserve to run and both deserve to report even
if only one is wrong, so a regression that corrupts the weight while leaving
the carrier correct shows up as exactly one failing line, not a test that
stops after the first problem and hides the second.

## Common assertions and EXPECT_THAT

The assertions that cover most test bodies:

| Macro | Checks |
|-------|--------|
| `EXPECT_EQ(a, b)` / `EXPECT_NE(a, b)` | equality / inequality |
| `EXPECT_LT`, `EXPECT_LE`, `EXPECT_GT`, `EXPECT_GE` | ordering |
| `EXPECT_TRUE(cond)` / `EXPECT_FALSE(cond)` | a boolean condition |
| `EXPECT_THROW(stmt, ExceptionType)` | `stmt` throws exactly `ExceptionType` |
| `EXPECT_NO_THROW(stmt)` | `stmt` throws nothing |
| `EXPECT_THAT(value, matcher)` | `value` satisfies a GoogleMock matcher |

Every one of these has an `ASSERT_*` counterpart with the same arguments and
the fatal-versus-nonfatal difference above.

```cpp
TEST(RateCardTest, UnresolvedZoneThrows) {
    RateCard card;
    EXPECT_THROW(card.rate_for_zone(ServiceZone::Unresolved), std::invalid_argument);
}
```

`EXPECT_THAT` is the one assertion that takes a matcher instead of a value to
compare against, and it is the entry point into GoogleMock's matcher library
even outside of `EXPECT_CALL`:

```cpp
TEST_F(ShipmentTrackerTest, FindsByPartialCarrierName) {
    tracker_.record(Shipment{"TRK-901", 1.0, swift_carrier_id_});

    const auto found = tracker_.find("TRK-901");

    EXPECT_THAT(found->carrier_name, ::testing::HasSubstr("Swift"));
}
```

`Eq`, `Gt`, `HasSubstr`, `Field`, `Property`, and `ElementsAre` are matchers
from the same library `EXPECT_CALL` uses to constrain an argument; the
GoogleMock appendix covers the full matcher vocabulary and how to combine
matchers with `EXPECT_CALL` actions. `EXPECT_THAT` on a plain value, with no
mock involved, is ordinary GoogleTest.

## Fixtures: SetUp, TearDown, and per-test lifecycle

A test fixture is a class deriving from `::testing::Test` that groups the
objects a related set of tests all need:

```cpp
class ShipmentTrackerTest : public ::testing::Test {
protected:
    void SetUp() override { clock_.set_now(reference_instant()); }
    void TearDown() override { tracker_.clear_for_test(); }

    FakeClock clock_;
    ShipmentTracker tracker_{clock_};
    CarrierId swift_carrier_id_{"swift-logistics"};
};
```

GoogleTest constructs a fresh `ShipmentTrackerTest` instance for every
`TEST_F` that uses it, runs `SetUp()`, runs the test body, runs `TearDown()`,
and destroys the instance, so member variables declared directly on the
fixture never leak state between tests on their own. `SetUp()`/`TearDown()`
are preferable to a constructor and destructor for two things a
constructor cannot do: resetting state that is not local to this fixture
instance, such as a shared static counter or an on-disk file, and running
cleanup that still executes when a fatal `ASSERT_*` cut the test body short.
A fatal assertion skips the rest of the test function and jumps straight to
`TearDown()`, so cleanup belongs there, not at the bottom of the test body
after the last assertion.

`SetUpTestSuite()` and `TearDownTestSuite()` are static methods on the same
fixture class for state that is expensive to build and safe to share read-only
across every test in the suite, built once rather than once per test:

```cpp
class CarrierRateTableTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { rate_table_ = new CarrierRateTable(load_reference_rates()); }
    static void TearDownTestSuite() { delete rate_table_; rate_table_ = nullptr; }

    static CarrierRateTable* rate_table_;
};
```

Reach for `SetUpTestSuite()` only when the setup is read-only for the whole
suite and expensive enough to matter, loading a reference rate table from
disk once instead of once per test case. A fixture member that any test
mutates belongs in `SetUp()` instead, so a mutation in one test cannot bleed
into the next.

## Parameterized tests: TestWithParam, GetParam, INSTANTIATE_TEST_SUITE_P

A parameterized test proves one behavior holds across a table of inputs,
without writing one `TEST` function per row. The fixture derives from
`::testing::TestWithParam<T>` instead of `::testing::Test`:

```cpp
struct CapacityCase {
    std::string name;
    double weight_kg;
    bool within_carrier_limit;
};

class CarrierCapacityTest : public ::testing::TestWithParam<CapacityCase> {};

TEST_P(CarrierCapacityTest, MatchesExpectedLimit) {
    const CapacityCase& test_case = GetParam();
    EXPECT_EQ(accepts_for_pickup(test_case.weight_kg), test_case.within_carrier_limit)
        << "case \"" << test_case.name << "\" disagreed with the carrier's payload limit";
}

INSTANTIATE_TEST_SUITE_P(
    PayloadLimits, CarrierCapacityTest,
    ::testing::ValuesIn(std::vector<CapacityCase>{
        {"well_under_limit", 10.0, true},
        {"exactly_at_limit", 50.0, true},
        {"one_gram_over", 50.001, false},
    }),
    [](const ::testing::TestParamInfo<CapacityCase>& info) { return info.param.name; });
```

`GetParam()` reads whichever row GoogleTest is currently driving the test
body with. The lambda passed to `INSTANTIATE_TEST_SUITE_P` names each
instance from the row's own `name` field, so `ctest -N` prints
`MatchesExpectedLimit/one_gram_over` instead of an index; without that
lambda, instances are numbered and a failure points at a row a reader has to
go count to identify. Reach for `TEST_P` when one behavior needs checking
against many input values; a fixture with `TEST_F` is the right tool when the
shape is the opposite, many behaviors sharing one setup.

## Typed and type-parameterized tests

A typed test proves a contract holds across more than one concrete type, when
the types share a method name and a promise but not a common base class.

```cpp
template <typename StrategyType>
class RateStrategyContractTest : public ::testing::Test {
protected:
    StrategyType strategy_;
};

using RateStrategyTypes = ::testing::Types<FlatRateStrategy, WeightTieredRateStrategy>;
TYPED_TEST_SUITE(RateStrategyContractTest, RateStrategyTypes);

TYPED_TEST(RateStrategyContractTest, NeverQuotesANegativeAmount) {
    for (double weight_kg : {0.0, 2.5, 500.0}) {
        EXPECT_GE(this->strategy_.rate_for(weight_kg), Cents{0});
    }
}
```

The fixture is a class template, instantiated once per entry in
`RateStrategyTypes`; GoogleTest runs every `TYPED_TEST` body against every
instantiation. Adding a third strategy later means adding its name to the
`::testing::Types` list, with no new test code.

`TYPED_TEST_SUITE` needs the full type list at the point the fixture is
declared. When the type list is not known until a later file includes a
shared contract header, perhaps a carrier plugin shipped and tested outside
this repository, `TYPED_TEST_P` plus `REGISTER_TYPED_TEST_SUITE_P` split the
contract's declaration from its instantiation: the contract header declares
the test bodies once with `TYPED_TEST_P` and registers them with
`REGISTER_TYPED_TEST_SUITE_P`, and each consumer instantiates that same
contract against its own type list with `INSTANTIATE_TYPED_TEST_SUITE_P`.
Reach for the plain `TYPED_TEST_SUITE` form unless the type list actually
has to live in more than one translation unit.

## Death tests: EXPECT_DEATH

A death test proves that a statement terminates the process, for a
precondition or invariant whose violation must not be recoverable:

```cpp
TEST(RateCardDeathTest, AbortsWhenCalledWithANegativeWeight) {
    EXPECT_DEATH(rate_for_weight(-1.0), "weight_kg must be non-negative");
}
```

`EXPECT_DEATH` runs the statement in a forked child process and checks both
that it dies and that it writes output matching the given pattern. Death
tests are slower and less parallel-friendly than an ordinary assertion
because of that fork, so reserve them for the handful of invariants that
should end the program, not for routine input validation that has a normal
return-value or exception contract already.

## Running through CTest

Every example registers its GoogleTest cases with `gtest_discover_tests`,
which asks the compiled test binary for its test list at build time and
registers each case with CTest individually, so a failure is reported per
case rather than per binary:

```sh
ctest --preset cpp23 --output-on-failure
```

The build and preset mechanics behind that command, the toolchain, Conan,
and the two presets, are covered in the
[build and toolchain reference]({{ '/docs/33-appendix-a-build-and-toolchain/' | relative_url }}).
GoogleMock's interaction-based mocks, `MOCK_METHOD` and `EXPECT_CALL`, are
covered in the
[mocking with gMock appendix]({{ '/docs/35-appendix-c-mocking-with-gmock/' | relative_url }}),
not here.

## For deeper coverage

- Hyuk Myeong, *Googletest in Practice*, self-published: the GoogleTest
  idioms this appendix and every example in this book are verified against.
- The official [GoogleTest documentation](https://google.github.io/googletest/)
  for the full macro and matcher reference.
- The official [GoogleTest primer](https://google.github.io/googletest/primer.html)
  for a from-scratch introduction to the framework's assertion model.

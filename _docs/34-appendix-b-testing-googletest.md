---
title: "Testing with GoogleTest"
order: 34
layout: tutorial
sectionid: docs
part: appendices
appendix: "B"
description: "A reference for the GoogleTest macros, assertions, fixtures, and parameterized/typed/death tests the examples use, and how they run through CTest."
duration: "15 min"
---

## Learning objectives

By the end of this appendix you can:

- Pick `TEST`, `TEST_F`, `TEST_P` or `TYPED_TEST` for a case.
- Choose between a fatal `ASSERT_*` and a nonfatal `EXPECT_*`.
- Put setup and cleanup in the fixture method that runs when you need it.
- Name parameterized instances so a failure points at its row.

## Why this matters

Every example's tests use GoogleTest. [Testing with GoogleTest]({{ '/docs/07-testing-with-googletest/' | relative_url }}) builds the foundation against a freight carrier's quote intake; this page is the lookup table. Mocking (`MOCK_METHOD`, `EXPECT_CALL`) is in [Appendix C]({{ '/docs/35-appendix-c-mocking-with-gmock/' | relative_url }}).

| Need | API |
| --- | --- |
| One independent case | `TEST(Suite, Name)` |
| Several cases sharing setup | `TEST_F(Fixture, Name)` |
| One behavior over a table of inputs | `TEST_P` + `INSTANTIATE_TEST_SUITE_P` |
| One contract over several types | `TYPED_TEST` + `TYPED_TEST_SUITE` |
| A contract instantiated in another translation unit | `TYPED_TEST_P` + `REGISTER_TYPED_TEST_SUITE_P` |
| A statement that must end the process | `EXPECT_DEATH` |

## TEST, TEST_F and TEST_P

`TEST(Suite, Name)` expands to a class derived from `::testing::Test`, with the macro body as its overridden `TestBody()`. A file-scope static object registers the class with GoogleTest's registry before `main()` runs, so `ctest -N` lists every case without running one: discovery walks the registry, not the bodies.

```cpp
TEST(RateCardTest, RegionalZoneUsesTheBaseRate) {
    RateCard card;
    EXPECT_EQ(card.rate_for_zone(ServiceZone::Regional), Cents{480});
}
```

| Macro | Generated class derives from | Body reads |
| --- | --- | --- |
| `TEST` | `::testing::Test` | nothing extra |
| `TEST_F(Fixture, Name)` | `Fixture` | the fixture's members, after `SetUp()` |
| `TEST_P(Fixture, Name)` | `Fixture`, itself a `::testing::TestWithParam<T>` | the current row, through `GetParam()` |

## EXPECT_* versus ASSERT_*

The two families differ only in what happens after a failure.

| Family | On failure |
| --- | --- |
| `EXPECT_*` | Records the failure; the test body keeps running. |
| `ASSERT_*` | Records the failure and returns from the enclosing function. `TearDown()` and the other tests still run. |

Use `ASSERT_*` where the next line would be undefined behavior or meaningless after a failure:

```cpp
TEST_F(ShipmentTrackerTest, RecordsAndReturnsTheStoredShipment) {
    tracker_.record(Shipment{"TRK-900", 4.2, carrier_id_});

    const auto found = tracker_.find("TRK-900");

    ASSERT_TRUE(found.has_value()) << "a shipment just recorded must be findable by its tracking id";
    EXPECT_EQ(found->weight_kg, 4.2);
    EXPECT_EQ(found->carrier, carrier_id_);
}
```

Dereferencing an empty `std::optional` is undefined behavior, so the presence check is fatal. The two field checks are independent, so each is nonfatal: a regression that corrupts only the weight reports one failing line, and the carrier check still runs.

## Common assertions and EXPECT_THAT

| Macro | Checks |
|-------|--------|
| `EXPECT_EQ(a, b)` / `EXPECT_NE(a, b)` | equality / inequality |
| `EXPECT_LT`, `EXPECT_LE`, `EXPECT_GT`, `EXPECT_GE` | ordering |
| `EXPECT_TRUE(cond)` / `EXPECT_FALSE(cond)` | a boolean condition |
| `EXPECT_THROW(stmt, ExceptionType)` | `stmt` throws exactly `ExceptionType` |
| `EXPECT_NO_THROW(stmt)` | `stmt` throws nothing |
| `EXPECT_THAT(value, matcher)` | `value` satisfies a GoogleMock matcher |

Each has an `ASSERT_*` counterpart with the same arguments.

```cpp
TEST(RateCardTest, UnresolvedZoneThrows) {
    RateCard card;
    EXPECT_THROW(card.rate_for_zone(ServiceZone::Unresolved), std::invalid_argument);
}
```

`EXPECT_THAT` takes a matcher instead of a value, and it's the entry point to GoogleMock's matcher library with no mock involved:

```cpp
TEST_F(ShipmentTrackerTest, FindsByPartialCarrierName) {
    tracker_.record(Shipment{"TRK-901", 1.0, swift_carrier_id_});

    const auto found = tracker_.find("TRK-901");

    EXPECT_THAT(found->carrier_name, ::testing::HasSubstr("Swift"));
}
```

`Eq`, `Gt`, `HasSubstr`, `Field`, `Property` and `ElementsAre` are the same matchers `EXPECT_CALL` uses to constrain arguments. The full vocabulary is in Appendix C.

## Fixtures: SetUp, TearDown and the per-test lifecycle

A fixture is a class deriving from `::testing::Test` that holds what a group of tests needs:

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

For every `TEST_F`, GoogleTest constructs a fresh fixture, runs `SetUp()`, the body and `TearDown()`, then destroys it. Members never carry state from one test to the next.

`SetUp()` and `TearDown()` do two things a constructor and destructor can't: reset state outside the instance, such as a static counter or a file on disk, and run cleanup after a fatal `ASSERT_*` cuts the body short. A fatal assertion jumps straight to `TearDown()`, so cleanup belongs there, not at the bottom of the body.

`SetUpTestSuite()` and `TearDownTestSuite()` are static and run once per suite, for state that's expensive to build and read-only across every test:

```cpp
class CarrierRateTableTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { rate_table_ = new CarrierRateTable(load_reference_rates()); }
    static void TearDownTestSuite() { delete rate_table_; rate_table_ = nullptr; }

    static CarrierRateTable* rate_table_;
};
```

| Method | Runs | Use for |
| --- | --- | --- |
| Constructor / `SetUp()` | Before each test | State any test may change |
| `TearDown()` | After each test, even after a fatal assertion | Cleanup |
| `SetUpTestSuite()` / `TearDownTestSuite()` | Once per suite | Expensive, read-only state, such as a reference rate table loaded from disk |

## Parameterized tests

A parameterized test checks one behavior across a table of inputs without one `TEST` per row. The fixture derives from `::testing::TestWithParam<T>`:

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

The last argument to `INSTANTIATE_TEST_SUITE_P` names each instance from its row, so `ctest -N` prints `MatchesExpectedLimit/one_gram_over` instead of an index. Without it, instances are numbered and a failure points at a row you have to count to find.

`TEST_P` is for one behavior over many inputs. `TEST_F` is the opposite shape: many behaviors over one setup.

## Typed and type-parameterized tests

A typed test checks one contract across several concrete types that share a method name and a promise but no common base class:

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

The fixture is a class template instantiated once per entry in the `::testing::Types` list, and every `TYPED_TEST` body runs against each instantiation. A third strategy is one more name in the list, with no new test code.

`TYPED_TEST_SUITE` needs the full type list where the fixture is declared. When the list isn't known until a later file includes a shared contract header, such as a carrier plugin tested outside this repository, split declaration from instantiation:

| Step | Where | Macro |
| --- | --- | --- |
| Declare the test bodies | Contract header | `TYPED_TEST_P` |
| Register them | Contract header | `REGISTER_TYPED_TEST_SUITE_P` |
| Instantiate against a type list | Each consumer | `INSTANTIATE_TYPED_TEST_SUITE_P` |

Use the plain `TYPED_TEST_SUITE` form unless the type list has to live in more than one translation unit.

## Death tests

A death test checks that a statement terminates the process, for a precondition whose violation must not be recoverable:

```cpp
TEST(RateCardDeathTest, AbortsWhenCalledWithANegativeWeight) {
    EXPECT_DEATH(rate_for_weight(-1.0), "weight_kg must be non-negative");
}
```

`EXPECT_DEATH` runs the statement in a forked child and checks that it dies with output matching the pattern. The fork makes death tests slower and less parallel-friendly than an ordinary assertion, so keep them for invariants that should end the program. Input validation with a return-value or exception contract uses an ordinary assertion.

## Running through CTest

`gtest_discover_tests` asks each compiled test binary for its list at build time and registers every case with CTest, so a failure is reported per case, not per binary:

```sh
ctest --preset cpp23 --output-on-failure
```

The toolchain, Conan and the two presets are in the [build and toolchain reference]({{ '/docs/33-appendix-a-build-and-toolchain/' | relative_url }}). `MOCK_METHOD` and `EXPECT_CALL` are in the [mocking with gMock appendix]({{ '/docs/35-appendix-c-mocking-with-gmock/' | relative_url }}).

## For deeper coverage

- Hyuk Myeong, *Googletest in Practice*, self-published: the GoogleTest
  idioms every example follows.
- The official [GoogleTest documentation](https://google.github.io/googletest/)
  for the full macro and matcher reference.
- The official [GoogleTest primer](https://google.github.io/googletest/primer.html)
  for a from-scratch introduction to the framework's assertion model.

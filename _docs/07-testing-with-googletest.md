---
title: "Proving it works: testing with GoogleTest"
order: 7
part: "cpp-substrate"
description: "The shared testing foundation every pattern in this book is verified against: GoogleTest fixtures, parameterized and typed tests, GoogleMock interaction tests, and death tests, taught against a freight carrier's quote intake component."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Choose between a plain `TEST` and a `TEST_F` fixture, and say what `SetUp`/`TearDown` buy over constructing objects inline.
- Replace near-identical `TEST` functions with one `TEST_P` suite driven by a table, with each case named so `ctest -N` reads like a requirements list.
- Check one contract across several concrete types with `TYPED_TEST_SUITE`.
- Use `ASSERT_*` where a failure must stop the test and `EXPECT_*` where it mustn't.
- Write a GoogleMock `EXPECT_CALL` with a matcher and an action to verify how a collaborator was called.
- Write an `EXPECT_DEATH` test for a precondition, and say why the precondition isn't checked with `assert()`.
- Recognize an over-mocked or tautological test, and say what to write instead.

## Why this matters

Every pattern chapter makes a claim: a Strategy prices a shipment the same way whichever mechanism quotes it, an `InvoiceRule` can be added without editing `InvoiceCalculator`, a subtype keeps its parent's promise. An unverified claim is worth nothing, which is why the build loop from Chapter 0 ends with `ctest`, not with "it compiled." GoogleTest is the only testing framework used, and its styles are covered once, here.

One style can't cover every claim:

| Claim | Style |
| --- | --- |
| a component with mutable state needs a clean start for each test | fixture, `TEST_F` |
| one function, many inputs and expected outputs | parameterized, `TEST_P` |
| several types honor one contract | typed, `TYPED_TEST` |
| a collaborator was called, or not, with the right arguments | GoogleMock, `EXPECT_CALL` |
| a violated precondition ends the program | death test, `EXPECT_DEATH` |

A freight carrier's quote intake uses each one where it fits.

{% include excalidraw.html name="07-test-styles" caption="Figure 7.1: Five GoogleTest/GoogleMock styles, each with the question it answers, funneling into the same CTest discovery and run flow." %}

## The domain: validating and recording a freight quote

The code is in `examples/07-testing-with-gtest`. A `ShipmentRequest` has a weight, a `ServiceZone` and a declared value. `QuoteIntake` decides whether to quote it, prices it, and asks a ledger to record the quote:

```cpp
struct ShipmentRequest {
    double weight_kg;
    ServiceZone zone;
    Cents declared_value_cents;
};

class QuoteIntake {
public:
    QuoteIntake(QuoteLedger& ledger, QuoteIdGenerator& id_generator);
    std::optional<Quote> submit(const ShipmentRequest& request);
private:
    QuoteLedger& ledger_;
    QuoteIdGenerator& id_generator_;
};
```

`QuoteIntake` takes its two collaborators, `QuoteLedger` and `QuoteIdGenerator`, as interfaces by reference, the same inversion as Chapter 4. That's what lets a test put a double in place of either one.

## A table of rules: the parameterized test

`validate_shipment()` applies three rules: the weight is positive, the zone is set, and the declared value is within the insurable limit. One `TEST` per rule plus one for the valid case means a new copy-pasted function for each new rule, and a reviewer reading five functions to confirm they agree on "valid." `TEST_P` with `INSTANTIATE_TEST_SUITE_P` makes it one assertion and a table:

```cpp
struct ValidationCase {
    std::string name;
    ShipmentRequest request;
    ValidationOutcome expected;
};

class ShipmentValidationTest : public ::testing::TestWithParam<ValidationCase> {};

TEST_P(ShipmentValidationTest, MatchesExpectedOutcome) {
    const ValidationCase& test_case = GetParam();
    EXPECT_EQ(validate_shipment(test_case.request), test_case.expected)
        << "case \"" << test_case.name << "\" did not resolve as expected";
}

INSTANTIATE_TEST_SUITE_P(
    IntakeRules, ShipmentValidationTest,
    ::testing::ValuesIn(std::vector<ValidationCase>{
        {"valid_regional_shipment", {3.5, ServiceZone::Regional, 12'000}, ValidationOutcome::Accepted},
        {"zero_weight_is_rejected", {0.0, ServiceZone::Local, 500}, ValidationOutcome::RejectedNegativeWeight},
        // ...
    }),
    [](const ::testing::TestParamInfo<ValidationCase>& info) { return info.param.name; });
```

Without the final lambda, GoogleTest names instances by index, `MatchesExpectedOutcome/0`, `/1`, and a failure sends the reader off counting rows. With it, `ctest -N` lists `MatchesExpectedOutcome/declared_value_over_the_limit_is_rejected`, and the failure explains itself. A new rule is a new row; the assertion exists once.

## One contract, two generators: the typed test

`QuoteIntake` needs quote ids, and there are two `QuoteIdGenerator` implementations: `SequentialQuoteIdGenerator`, which formats ids from a process-wide counter, and `PrefixedQuoteIdGenerator`, which uses an instance counter and a prefix. Both promise that `next_id()` never returns an empty string and never repeats. That's a substitutability claim like Chapter 3's, checked against a list of concrete types. `TYPED_TEST_SUITE` runs the same checks for each:

```cpp
template <typename GeneratorType>
class QuoteIdGeneratorContractTest : public ::testing::Test {
protected:
    GeneratorType generator_;
};

using QuoteIdGeneratorTypes = ::testing::Types<SequentialQuoteIdGenerator, PrefixedQuoteIdGenerator>;
TYPED_TEST_SUITE(QuoteIdGeneratorContractTest, QuoteIdGeneratorTypes);

TYPED_TEST(QuoteIdGeneratorContractTest, NeverRepeatsAcrossOneHundredCalls) {
    std::unordered_set<std::string> seen;
    for (int call = 0; call < 100; ++call) {
        const bool first_time_seen = seen.insert(this->generator_.next_id()).second;
        ASSERT_TRUE(first_time_seen) << "a duplicate id was produced on call " << call;
    }
}
```

The fixture is a template, instantiated once per type in the list, and every `TYPED_TEST` body runs against each. A third generator, say one backed by a UUID library, is one more name in `::testing::Types`. The fixture instantiates the concrete type directly, so the types don't need a common base; any type with a matching `next_id()` works.

| Shape | Style |
| --- | --- |
| one type, many input values | parameterized |
| many types, one contract | typed |

## Shared state: the fixture

Every intake test needs a `QuoteLedger`, a `QuoteIdGenerator`, a `QuoteIntake` wired to both, and usually the same valid request. A `TEST_F` fixture declares them once instead of in each of the four tests:

```cpp
class QuoteIntakeTest : public ::testing::Test {
protected:
    void SetUp() override { SequentialQuoteIdGenerator::reset_sequence_for_test(1); }
    void TearDown() override { SequentialQuoteIdGenerator::reset_sequence_for_test(1); }

    MockQuoteLedger ledger_;
    SequentialQuoteIdGenerator id_generator_;
    QuoteIntake intake_{ledger_, id_generator_};
    const ShipmentRequest valid_request_{3.5, ServiceZone::Regional, 12'000};
};
```

GoogleTest builds a fresh fixture object for every `TEST_F`, so members alone would be enough for per-instance state. `SequentialQuoteIdGenerator`'s counter is static instead, because the real sequence lives in a database shared by every `QuoteIntake` in the process. A fresh member doesn't reset it, and without `SetUp()` the id a test sees would depend on how many ids earlier tests consumed.

`TearDown()` resets it too. A fatal assertion skips the rest of the test body and goes straight to `TearDown()`, so cleanup written after the last assertion would never run on exactly the runs where something went wrong.

## `ASSERT_*` versus `EXPECT_*`

`EXPECT_*` records a failure and keeps going. `ASSERT_*` records it and returns from the test function.

```cpp
TEST_F(QuoteIntakeTest, AcceptsAndRecordsAValidShipment) {
    EXPECT_CALL(ledger_, record(_)).WillOnce(Return(true));

    const auto result = intake_.submit(valid_request_);

    ASSERT_TRUE(result.has_value()) << "a valid shipment must produce a quote, not std::nullopt";
    EXPECT_EQ(result->amount_cents, 1225) << "pricing changed; check quote_cost()'s weight and zone math";
    EXPECT_EQ(result->quote_id, "Q-0001");
}
```

| Check | Family | Why |
| --- | --- | --- |
| `result.has_value()` | `ASSERT_TRUE` | if `submit()` returned `std::nullopt`, `result->amount_cents` would be undefined behavior, not a clean failure |
| the price and the id | `EXPECT_EQ` | independent facts; a regression in one shouldn't hide the other |

The streamed message prints only on failure, so the reader sees `check quote_cost()'s weight and zone math` instead of a bare `1225 != 980`.

## Verifying the call: GoogleMock

A return value can't show whether `QuoteIntake` talked to its ledger correctly. `MockQuoteLedger` implements `QuoteLedger` with one mocked method:

```cpp
class MockQuoteLedger : public QuoteLedger {
public:
    MOCK_METHOD(bool, record, (const Quote& quote), (override));
};
```

`EXPECT_CALL` sets an expectation before the code runs, and the test fails if it isn't met by the time the mock is destroyed:

```cpp
TEST_F(QuoteIntakeTest, RejectsAnInvalidShipmentWithoutTouchingTheLedger) {
    EXPECT_CALL(ledger_, record(_)).Times(0);
    const ShipmentRequest malformed{-1.0, ServiceZone::Local, 500};

    const auto result = intake_.submit(malformed);

    EXPECT_FALSE(result.has_value());
}
```

`Times(0)` makes the interaction the assertion: an invalid shipment never reaches the ledger. Checking only `result.has_value()` would pass even if `QuoteIntake` called `record()` and ignored the answer. A matcher on the argument goes further:

```cpp
EXPECT_CALL(ledger_, record(Field(&Quote::amount_cents, Eq(1225)))).WillOnce(Return(true));
```

| Piece | Does |
| --- | --- |
| `Field(&Quote::amount_cents, ...)` | matches one member of the `Quote` argument |
| `Eq(1225)` | pins its value |
| `WillOnce(Return(true))` | the action: what the mock returns to `QuoteIntake` |

That catches `QuoteIntake` recording the wrong price while returning the right one to its caller, which no return-value check can see. Returning `false` from the same action shows that `submit()` returns `std::nullopt` when the ledger declines, not only when validation fails.

## A failure that must not be recoverable: the death test

`quote_cost()` assumes its caller already validated the request and that `weight_kg` is non-negative. Called directly with a malformed request, it must abort, not return a nonsense price. `EXPECT_DEATH` runs the statement in a child process and checks that it dies with a matching message:

```cpp
TEST(QuoteCostDeathTest, AbortsWhenCalledWithANegativeWeightPrecondition) {
    const ShipmentRequest malformed{-1.0, ServiceZone::Local, 500};
    EXPECT_DEATH(quote_cost(malformed), "weight_kg must be non-negative");
}
```

The check isn't `assert()`. Both presets set `CMAKE_BUILD_TYPE` to `Release`, CMake's Release flags add `-DNDEBUG`, and `assert()` compiles to nothing. `quote_cost()` uses `FREIGHT_REQUIRE`, which aborts in every configuration:

```cpp
#define FREIGHT_REQUIRE(condition, message)                                  \
    do {                                                                     \
        if (!(condition)) {                                                 \
            std::fprintf(stderr, "FREIGHT_REQUIRE failed: %s\n", (message)); \
            std::abort();                                                   \
        }                                                                    \
    } while (false)
```

Use a death test when a violation is meant to be fatal. A check that `quote_cost()` threw or returned a sentinel would test a gentler contract than the one it promises. Death tests fork, so they're slower and less parallel-friendly; keep them for the few invariants that should end the program, not for input validation with a normal return-value contract.

> **🤖 Explore with your agent**
>
> "In `examples/07-testing-with-gtest`, add cases for `QuoteIntake::submit()` to `tests/quote_intake_test.cpp` using the existing `QuoteIntakeTest` fixture and `MockQuoteLedger` from `tests/mock_quote_ledger.hpp`. For an invalid request, expect `record(_)` with `Times(0)`. For a valid one, match the argument with `Field(&Quote::amount_cents, Eq(...))` using the price `quote_cost()` gives. Use `ASSERT_TRUE` on `has_value()` before dereferencing the result. Don't mock anything in the `validate_shipment()` or `quote_cost()` cases; call them directly."
>
> Check with `ctest --preset cpp23 -R QuoteIntake`, and reject any case whose only assertion restates what the mock was told to return.

## When not to use these styles

| Style | Skip it when | Write instead |
| --- | --- | --- |
| `TEST_F` | there's no shared mutable state and no costly setup | plain `TEST`s that each build what they need; no scrolling back to member declarations |
| `TEST_P` | there's only one case | a `TEST`; a one-row instantiation is machinery around nothing |
| typed test | only one type exists and no second one is coming | a `TEST`; the typed contract is justified here because `PrefixedQuoteIdGenerator` exists |
| GoogleMock | the collaborator is pure, like `quote_cost()` or `validate_shipment()` | call the real function and assert on its result |

Mock `QuoteLedger` because `record()` has a side effect, writing to storage, that a real implementation would need a database to observe. Mocking a pure function makes the test slower to read and no more trustworthy.

## Build, run, observe

From the repository root:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

All five styles land in one binary and are discovered the same way. There's no C++23-only code, so all 15 cases pass under both presets. `ctest -N` lists each parameterized case by the name its instantiation gave it:

```
IntakeRules/ShipmentValidationTest.MatchesExpectedOutcome/valid_regional_shipment
IntakeRules/ShipmentValidationTest.MatchesExpectedOutcome/zero_weight_is_rejected
IntakeRules/ShipmentValidationTest.MatchesExpectedOutcome/declared_value_over_the_limit_is_rejected
```

Then run the demo:

```sh
examples/07-testing-with-gtest/demo.sh cpp23
```

It validates five requests, submits two, then seeds the ledger with the next id so a third submission is declined by the ledger rather than by validation:

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

Under `cpp20` the output is identical.

- **Use `assert()`.** In a scratch copy, replace the `FREIGHT_REQUIRE` call in `quote_cost()` with `assert(request.weight_kg >= 0.0 && "...")`. The death test fails: Release defines `NDEBUG`, the check disappears, and `quote_cost()` returns a price instead of aborting.

## For deeper coverage

- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official
  [GoogleTest documentation](https://google.github.io/googletest/): the
  testing idiom behind every example in this book, including the fixture,
  parameterized, typed, mock, and death-test styles this chapter trains,
  plus `gtest_discover_tests`'s CTest integration and GoogleMock's full
  matcher and action vocabulary beyond the handful this chapter used.
- The full reading list behind this book's opinions lives on the
  [bibliography page]({{ '/bibliography/' | relative_url }}); each
  citation there is a pointer to the full book, not a substitute for
  reading it.

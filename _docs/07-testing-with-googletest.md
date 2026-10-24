---
title: "Proving it works: testing with GoogleTest"
order: 7
part: "cpp-substrate"
description: "The shared testing foundation every pattern in this book is verified against: GoogleTest fixtures, parameterized and typed tests, GoogleMock interaction tests, and death tests, taught against a freight carrier's quote intake component."
duration: "30 min"
---

## Learning objectives

By the end of this chapter you can:

- Choose between a plain `TEST` and a `TEST_F` fixture, and say what a
  `SetUp`/`TearDown` pair buys you that constructing an object inline in
  the test body does not.
- Replace a handful of near-identical `TEST` functions with one `TEST_P`
  suite driven by a table of cases, and name each case so `ctest -N`
  reads like a requirements list.
- Prove the same contract holds across more than one concrete type with a
  `TYPED_TEST_SUITE`, instead of writing the same checks out once per type.
- Use `ASSERT_*` where a failure must stop the test immediately and
  `EXPECT_*` where it must not, and explain the difference in terms of
  what runs after each one fails.
- Write a GoogleMock `EXPECT_CALL` with a matcher and an action to verify
  that a collaborator was called correctly, not only that a function
  returned the right value.
- Write an `EXPECT_DEATH` test for a precondition, and say why this
  book's examples check that precondition with something other than the
  standard `assert()` macro.
- Recognize an over-mocked or tautological test, including one an LLM
  pairing partner is likely to hand you, and say what to write instead.

## Why this matters

Every pattern chapter in this book makes a specific claim: this Strategy
prices a shipment the same way no matter which of three mechanisms quotes
it, this `InvoiceRule` can be added without editing `InvoiceCalculator`,
this subtype keeps its parent's promise under substitution. None of those
claims hold any weight the moment they are unverified, which is why
Chapter 0's build-and-test loop ends every edit with `ctest`, not with
"it compiled." GoogleTest is the only testing idiom this book uses, and
this chapter is where the book trains it once, so every pattern chapter
after this one can spend its test suite proving the pattern's claim
instead of re-explaining how a fixture or a mock works.

That single foundation has to cover more ground than one test style can.
Some components carry mutable state a test needs to reset between runs.
Some behaviors are best proven as a table of inputs and expected outputs
rather than five near-identical functions. Some claims are about a
contract that more than one concrete type must honor. Some failures are
about whether a collaborator was called correctly, not just about a
return value. And some failures are not supposed to be recoverable at
all; a violated precondition should stop the program, and a test needs a
way to assert that it does. GoogleTest and GoogleMock give this book one
mechanism for each of those five shapes, and the component this chapter
builds, a freight carrier's quote intake, uses every one of them where it
fits a real need in the suite, never for completeness.

{% include excalidraw.html name="07-test-styles" caption="Figure 7.1: Five GoogleTest/GoogleMock styles, each with the question it answers, funneling into the same CTest discovery and run flow." %}

## The domain: validating and recording a freight quote

The example behind this chapter lives in `examples/07-testing-with-gtest`.
A `ShipmentRequest` arrives with a weight, a `ServiceZone`, and a declared
value; `QuoteIntake` is the component that decides whether to quote it,
prices it if so, and asks a ledger to make the quote durable:

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

`submit()` validates the request, and only an accepted request goes on to
be priced and handed to the ledger. `QuoteIntake` depends on two
collaborators through interfaces it never implements itself,
`QuoteLedger` and `QuoteIdGenerator`, injected by reference and owned by
the caller. That dependency shape, the same inversion Chapter 4 trained
on a notification gateway, is what lets this chapter's tests swap a real
collaborator for a test double without `QuoteIntake`'s own code noticing
the difference. Every GoogleTest style below proves one fact about this
component; none of them duplicate each other's job.

## A table of rules: the parameterized test

`validate_shipment()` checks a `ShipmentRequest` against three
independent rules: weight must be positive, the destination zone must be
resolved, and the declared value must sit under the carrier's insurable
limit. Writing one `TEST` function per rule, plus one for the shipment
that passes all three, would work, but it scales badly: a fourth rule
means a fourth copy-pasted function, and a reviewer has to read five
functions to confirm the five cases agree on what "valid" means. `TEST_P`
plus `INSTANTIATE_TEST_SUITE_P` collapses that into one assertion and a
table:

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

The lambda in the last argument is doing real work, not decoration.
Without it, GoogleTest names each instance by its index in the table,
`MatchesExpectedOutcome/0`, `MatchesExpectedOutcome/1`, and so on; a
failing test tells a reader which number broke, and the reader has to go
count rows in the table to find out what that number means. With it,
`ctest -N` prints `MatchesExpectedOutcome/declared_value_over_the_limit_is_rejected`,
and a failure is legible from the test name alone, before anyone opens
the source file. Adding a sixth intake rule later means adding a row to
the table, not a new function; the assertion logic, and the risk of a
copy-paste mistake between near-identical functions, exists exactly once.

## One contract, two generators: the typed test

`QuoteIntake` needs a way to hand out quote ids, and this example ships
two: `SequentialQuoteIdGenerator`, which formats an id from a
process-wide counter, and `PrefixedQuoteIdGenerator`, which formats one
from an instance-local counter with a different prefix. Both satisfy the
same unwritten contract: `next_id()` never returns an empty string, and
never repeats within the process. That contract is exactly the kind of
claim Chapter 3 trained a test to make concrete for Liskov Substitution,
a function written once against a base type and called with every
subtype, except here the two types do not share a base class at all;
they only share a method name and a promise.

`TYPED_TEST_SUITE` is GoogleTest's answer to proving a shared contract
against a type list, without inheritance tying the types together:

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

The fixture class is a template, instantiated once per entry in
`QuoteIdGeneratorTypes`, and GoogleTest runs every `TYPED_TEST` body
against every instantiation. Adding a third generator later, say one
backed by a UUID library, means adding its name to the `::testing::Types`
list; the two checks above run against it automatically, with no new test
code. Reach for a typed test specifically when the thing under test is a
contract more than one concrete type must honor; a parameterized test
covers the opposite shape, one type and many input values, and the two
are not substitutes for each other.

## Shared state across tests: the fixture

`QuoteIntakeTest` is where the test suite needs more than a value table.
Every test in this suite needs a `QuoteLedger`, a `QuoteIdGenerator`, and
a `QuoteIntake` wired to both, and most of them reuse the same baseline
`ShipmentRequest`. A plain `TEST` could construct all four inline at the
top of every function, but that duplicates four lines across nine tests
for no benefit; a `TEST_F` fixture builds them once as protected members:

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

The member declarations alone would be enough if every piece of state
this fixture touches were local to one `QuoteIntakeTest` instance, since
GoogleTest constructs a fresh fixture object for every `TEST_F`. What
makes `SetUp`/`TearDown` necessary here is that
`SequentialQuoteIdGenerator`'s counter is not local to one instance; it
is a static member, because a real backing sequence lives in a database
shared across every `QuoteIntake` in the process, not inside whichever
generator object happens to be running. A fresh fixture
member does not reset that shared counter, so without an explicit reset
the quote id a test observes would depend on how many ids every earlier
test in the binary already consumed. `SetUp()` puts the counter back to a
known value before the test body runs.

`TearDown()` resets it again, not only `SetUp()`, and the reason is the
same fact the next section trains directly: a fatal assertion failure
partway through a test body skips the rest of that body and jumps
straight to `TearDown()`. Cleanup written only at the bottom of a test
function, after the last assertion, would silently never run on exactly
the test run where something already went wrong. Putting the reset in
`TearDown()` instead guarantees it runs whether the test passed, failed
non-fatally, or aborted early on a fatal check.

## Choosing when a failure must stop the test: `ASSERT_*` versus `EXPECT_*`

GoogleTest's two assertion families differ in exactly one way: what
happens immediately after one of them fails. `EXPECT_*` records the
failure and lets the test function keep running; `ASSERT_*` records the
failure and returns from the test function on the spot, skipping
everything after it. That difference is a design decision, not a style
preference, and `QuoteIntakeTest`'s first test makes the choice visible:

```cpp
TEST_F(QuoteIntakeTest, AcceptsAndRecordsAValidShipment) {
    EXPECT_CALL(ledger_, record(_)).WillOnce(Return(true));

    const auto result = intake_.submit(valid_request_);

    ASSERT_TRUE(result.has_value()) << "a valid shipment must produce a quote, not std::nullopt";
    EXPECT_EQ(result->amount_cents, 1225) << "pricing changed; check quote_cost()'s weight and zone math";
    EXPECT_EQ(result->quote_id, "Q-0001");
}
```

`result` is a `std::optional<Quote>`. If `submit()` returned
`std::nullopt`, the line `result->amount_cents` would be undefined
behavior, not a clean test failure; the test has to stop before it gets
there, which is exactly what `ASSERT_TRUE` does. The two `EXPECT_EQ`
checks after it are independent facts about the same quote, the price
and the id, and both deserve to run and both deserve to report even if
only one of them is wrong; a regression that breaks the price while
leaving the id correct, or the reverse, shows up as one failing line
instead of a test that stops after the first problem and hides the
second. The streamed message on each assertion is doing real work too:
`ASSERT_TRUE(...) << "..."` prints that string only when the check fails,
so a future reader sees a sentence that names what probably broke,
`check quote_cost()'s weight and zone math`, instead of a bare `1225 !=
980` with no context pointing at where to look.

## Verifying the call, not just the result: GoogleMock

Every test above this one asks what `submit()` returns. The next
question GoogleTest alone cannot answer is whether `QuoteIntake` talked
to its ledger correctly, and that is GoogleMock's job. `MockQuoteLedger`
implements the same `QuoteLedger` interface the real ledger does, with
one method turned into a mock:

```cpp
class MockQuoteLedger : public QuoteLedger {
public:
    MOCK_METHOD(bool, record, (const Quote& quote), (override));
};
```

`EXPECT_CALL` sets an expectation on that mock before the code under test
runs, and the test fails if the expectation is not met by the time the
mock is destroyed:

```cpp
TEST_F(QuoteIntakeTest, RejectsAnInvalidShipmentWithoutTouchingTheLedger) {
    EXPECT_CALL(ledger_, record(_)).Times(0);
    const ShipmentRequest malformed{-1.0, ServiceZone::Local, 500};

    const auto result = intake_.submit(malformed);

    EXPECT_FALSE(result.has_value());
}
```

`Times(0)` makes the interaction itself the assertion: a shipment that
fails validation must never reach the ledger at all, not reach it and get
declined. A test that only checked `result.has_value()` would pass even
if `QuoteIntake` called `record()` first and ignored the answer, a bug
this interaction check catches and a return-value check cannot.
`PassesTheComputedAmountToTheLedgerBeforeRecording` goes one step
further, matching on the argument `record()` receives:

```cpp
EXPECT_CALL(ledger_, record(Field(&Quote::amount_cents, Eq(1225)))).WillOnce(Return(true));
```

`Field` reaches into the `Quote` argument and matches on one member,
`Eq(1225)` pins the exact value, and `WillOnce(Return(true))` is the
action, telling the mock what to hand back to `QuoteIntake` when the call
arrives. This is the check that would catch a bug where `QuoteIntake`
records the wrong price while still returning the right one to its own
caller, a class of bug a test that only inspects `submit()`'s return
value structurally cannot see. `ReturnsNulloptWhenTheLedgerDeclinesToRecord`
uses the same action mechanism the other way, returning `false` from the
mock to prove that `submit()` defers to the ledger's answer and not only
to its own validation.

## A failure that must not be recoverable: the death test

`quote_cost()` has a precondition: its caller must have already validated
the request, because this function assumes `weight_kg` is non-negative
and does not check it the way `validate_shipment()` does. Calling it
directly with a malformed request, bypassing validation the way a
careless future caller might, must not silently return a nonsense price;
it must abort. `EXPECT_DEATH` is GoogleTest's way of asserting that a
statement terminates the process, running the statement in a forked
child process and checking both that it dies and that it prints a
matching message:

```cpp
TEST(QuoteCostDeathTest, AbortsWhenCalledWithANegativeWeightPrecondition) {
    const ShipmentRequest malformed{-1.0, ServiceZone::Local, 500};
    EXPECT_DEATH(quote_cost(malformed), "weight_kg must be non-negative");
}
```

The precondition check behind this test is not the standard library's
`assert()`. This book's CMake presets build every example as Release
(`CMakePresets.json`'s `cpp23` and `cpp20` presets both set
`CMAKE_BUILD_TYPE: Release`), and CMake's default Release flags add
`-DNDEBUG`, which compiles `assert()` down to nothing. A precondition
that only fires in a debug build nobody runs in CI is not checked
anywhere this book's examples ship, so `quote_cost()` guards
itself with `FREIGHT_REQUIRE`, a small macro that checks its condition
and calls `std::abort()` unconditionally, in every build configuration:

```cpp
#define FREIGHT_REQUIRE(condition, message)                                  \
    do {                                                                     \
        if (!(condition)) {                                                 \
            std::fprintf(stderr, "FREIGHT_REQUIRE failed: %s\n", (message)); \
            std::abort();                                                   \
        }                                                                    \
    } while (false)
```

Reach for a death test specifically when a precondition or an invariant
is supposed to be fatal, not recoverable; a test that only confirmed
`quote_cost()` threw an exception or returned a sentinel for the same
input would be testing a different, gentler contract than the one this
function promises. Death tests run the checked statement in a
subprocess, so they are slower and less parallel-friendly than an
ordinary assertion; reserve them for the handful of invariants whose
violation should end the program, not for routine input validation that
has a normal return-value contract already.

> **AI sidebar.** Ask an LLM pairing partner to "write tests for
> `QuoteIntake`" and a common result compiles, runs green, and proves
> almost nothing. Two failure shapes show up constantly enough to check
> for by name. The first is the tautological test: an assertion that
> restates the code under test rather than an independent expectation,
> for instance mocking `QuoteLedger::record()` to always return `true`
> and then asserting `submit()` returned a value, which only proves the
> mock did what it was told, not that `QuoteIntake` behaves correctly.
> The second is over-mocking: a generated test that mocks `QuoteLedger`
> and `QuoteIdGenerator` even for `ShipmentValidationTest`'s pure
> `validate_shipment()` checks, which need neither collaborator and
> should stay plain `TEST_P` cases with zero mocks. Read every generated
> `EXPECT_CALL` the way this chapter trained: is this checking a real
> interaction a bug could break, or is it restating what the test just
> told the mock to do? A generated suite with high line coverage and zero
> `Times(0)` or argument-matcher expectations is a strong sign the model
> reached for the mechanics of GoogleMock without reaching for the actual
> question an interaction test exists to answer.

## When not to use these styles

Every style in this chapter pays for itself when the shape it answers is
present in the component under test, and each one has a plain
alternative that is the better engineering choice everywhere else.

A `TEST_F` fixture with no shared mutable state and no expensive setup is
solving a problem the test suite does not have; a handful of independent
`TEST` functions, each constructing the one or two objects it needs
inline, is easier to read top to bottom than a fixture whose member
declarations a reader has to scroll back to before understanding any one
test. `TEST_P` pays off only once a second or third case of the
same assertion shows up; writing a one-row `INSTANTIATE_TEST_SUITE_P` for
a single case adds machinery around a test that could have stayed a
`TEST`. A typed test is premature when only one concrete type exists and
no second one is coming; `QuoteIdGeneratorContractTest` is justified here
because `PrefixedQuoteIdGenerator` already exists as a second
implementation, not because typed tests are a more sophisticated-looking
way to test one generator.

GoogleMock's cost is the easiest one to overspend without noticing. A
collaborator with no side effect worth checking, a pure function like
`quote_cost()` or `validate_shipment()`, never needs a mock; calling the
real function with a real input and asserting on the real return value is
both the simplest test to write and the one closest to what users of the
function depend on. Reach for `MockQuoteLedger` only because
`QuoteLedger::record()` has a side effect, writing a quote to storage,
that a real implementation's test would need a real database or a slow
integration test to observe. Mocking a pure function to "isolate" a test
from code that has nothing to isolate from makes the test slower to read
and no more trustworthy than calling the function directly.

## Build, run, observe

Every command below runs with the local clang, CMake, Ninja, and Conan
toolchain Chapter 0 installs; there is no container anywhere in this
loop. From the repository root, the root superbuild picks up
`examples/07-testing-with-gtest` automatically:

```sh
scripts/build.sh cpp23 && scripts/verify.sh cpp23
scripts/build.sh cpp20 && scripts/verify.sh cpp20
```

`scripts/verify.sh` runs CTest, which discovers this example's test cases
through `gtest_discover_tests`: all five styles in this chapter, the
fixture, the parameterized suite, the typed suite, the mock interaction
tests, and the death test, land in one binary and are discovered the same
way. This example carries no C++23-only code, so the count does not
change between presets, 15 cases under both. Observed locally on Fedora
44 / clang 22.1.8:

```
100% tests passed, 0 tests failed out of 15   # cpp23
100% tests passed, 0 tests failed out of 15   # cpp20
```

`ctest -N` shows each parameterized case under the name its instantiation
gave it, not a bare index, which is the payoff of the name-generator
lambda from earlier in this chapter:

```
IntakeRules/ShipmentValidationTest.MatchesExpectedOutcome/valid_regional_shipment
IntakeRules/ShipmentValidationTest.MatchesExpectedOutcome/zero_weight_is_rejected
IntakeRules/ShipmentValidationTest.MatchesExpectedOutcome/declared_value_over_the_limit_is_rejected
```

To see the component validate, price, and record a shipment rather than
just reading an assertion that says it works, run the example's own demo
script:

```sh
examples/07-testing-with-gtest/demo.sh cpp23
```

It validates five sample requests, submits two of them through intake,
and then seeds the ledger with the id the generator is about to hand out
next, so a third submission is rejected by the ledger itself rather than
by validation:

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

Running `examples/07-testing-with-gtest/demo.sh cpp20` prints the
identical output; nothing in this example's demo path is standard-specific.

## For deeper coverage

- *GoogleTest in Practice* (Hyuk Myeong, Manning) and the official
  [GoogleTest documentation](https://google.github.io/googletest/): the
  testing idiom behind every example in this book, including the fixture,
  parameterized, typed, mock, and death-test styles this chapter trains,
  plus `gtest_discover_tests`'s CTest integration and GoogleMock's full
  matcher and action vocabulary beyond the handful this chapter used.
- The full reading list behind this book's opinions lives on the
  [bibliography page]({{ '/bibliography/' | relative_url }}); each
  citation there is a pointer to the full book, not a substitute for
  reading it.

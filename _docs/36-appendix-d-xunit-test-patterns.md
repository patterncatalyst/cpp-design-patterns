---
title: "xUnit test patterns"
order: 36
layout: tutorial
sectionid: docs
part: appendices
appendix: "D"
description: "Gerard Meszaros's xUnit test patterns, the four-phase test, the test-double taxonomy, fixture strategies, and the common test smells, mapped onto GoogleTest and gMock."
duration: "15 min"
---

## Learning objectives

By the end of this appendix you can:

- Map the four phases of a test onto a GoogleTest fixture and `TEST_F` body.
- Name the five test doubles and build each one with GoogleTest and gMock.
- Choose a fresh, shared or persistent fixture and reset process-wide state.
- Recognize six common test smells and apply the fix.

## Why this matters

Gerard Meszaros's *xUnit Test Patterns* gave testing a shared vocabulary: four-phase test, test double, fixture, test smell. This page maps each term onto GoogleTest and gMock, using the collaborators from the examples: a quote ledger, a tracking store, a carrier gateway, a clock.

## The four-phase test

| Phase | Also called | Does | GoogleTest home |
| --- | --- | --- | --- |
| Setup | Arrange | Builds the object under test and its collaborators | Fixture members, constructor or `SetUp()`, plus any `EXPECT_CALL` |
| Exercise | Act | Calls the one behavior under test | `TEST_F` body |
| Verify | Assert | Checks the outcome | `TEST_F` body |
| Teardown | | Releases what setup acquired, resets global state | Destructor or `TearDown()`, which runs even after a failure |

A fixture moves setup and teardown out of the body, so a `TEST_F` holds only exercise and verify:

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

TEST_F(QuoteIntakeTest, AcceptsAndRecordsAValidShipment) {
    EXPECT_CALL(ledger_, record(_)).WillOnce(Return(true));   // setup continues: expectation

    const auto result = intake_.submit(valid_request_);        // exercise

    ASSERT_TRUE(result.has_value());                            // verify
    EXPECT_EQ(result->amount_cents, 1225);                      // verify
}
```

An `EXPECT_CALL` is setup, not verify, though it reads like an assertion: it declares what the mock does before the call happens. Fixture members are setup too, built fresh for every test because GoogleTest constructs a new fixture instance per test. The full fixture is in [Testing with GoogleTest]({{ '/docs/07-testing-with-googletest/' | relative_url }}).

## The test-double taxonomy

| Double | What it does | In this stack |
|---|---|---|
| **Dummy** | A placeholder that satisfies a signature but is never exercised | A `CarrierGateway&` passed into a constructor whose code path under test never calls it |
| **Stub** | Returns canned values on request, no verification of how it was called | A gMock `ON_CALL` on `Clock::now()`, or a hand-written `StubTrackingStore` that always returns one fixed manifest |
| **Spy** | Records what was called, inspected after the fact without a prior expectation | A hand-written `SpyCarrierGateway` that appends every `submit_label_request()` argument to a vector read after exercise |
| **Mock** | Verifies expected interactions as part of the test, failing if they are not met | `gMock`'s `EXPECT_CALL`, as `MockQuoteLedger` uses on `record()` |
| **Fake** | A working, lightweight implementation that behaves correctly but is unsuited to production | An in-memory `InMemoryTrackingStore` backed by a hash map, used in place of a real store connection |

### Dummy

`QuoteIntake` takes a `QuoteLedger&` and a `QuoteIdGenerator&`. Where only id generation matters, the ledger can be a dummy: a minimal subclass whose `record()` returns `true` and nothing else.

### Stub

A `Clock` stub pins "now" to one instant so a shipment SLA checker's deadline math is deterministic:

```cpp
ON_CALL(clock_, now()).WillByDefault(Return(fixed_instant));
```

There's no `EXPECT_CALL`: the number of `now()` calls doesn't matter, only the answer.

### Spy

A spy records every request instead of returning a scripted answer:

```cpp
class SpyCarrierGateway : public CarrierGateway {
public:
    LabelReceipt submit_label_request(const LabelRequest& request) override {
        requests_seen.push_back(request);
        return LabelReceipt{"LBL-SPY-0001"};
    }
    std::vector<LabelRequest> requests_seen;
};
```

Exercise the dispatcher, then assert against `requests_seen`. A spy fits when "look at what accumulated" is easier to state than an expectation declared up front.

### Mock

`EXPECT_CALL` makes the interaction the assertion. A dispatcher that must not retry after a non-retryable carrier rejection:

```cpp
EXPECT_CALL(carrier_gateway_, submit_label_request(_)).Times(1);
```

### Fake

`InMemoryTrackingStore` is a working implementation that records events and returns them, keeping everything in a `std::unordered_map` instead of opening a connection. The real `TrackingStoreConnection` is in [Resource pools]({{ '/docs/24-resource-pools/' | relative_url }}). Write a fake when a stub's one canned value is too thin, such as a tracking query that depends on events recorded earlier in the same test.

## Fixture strategies

| Strategy | Lifetime | Use when | Risk |
| --- | --- | --- | --- |
| Fresh fixture | New instance per `TEST_F`, the GoogleTest default | Always, by default | None between tests |
| Shared (suite) fixture | `static` members built in `SetUpTestSuite()`, released in `TearDownTestSuite()` | The collaborator is expensive and read-only, such as a `CarrierRateCard` loaded once from a fixed table | State one test leaves behind leaks into the next, and test order stops being safe to shuffle |
| Transient fixture | Starts empty, discarded after the test | An `InMemoryTrackingStore` built per test | None |
| Persistent fixture | Survives across tests or runs | A file-backed `TrackingStore` seeded once | Needs an explicit reset |

A fresh fixture object doesn't reset process-wide state. `SequentialQuoteIdGenerator`'s counter is static, so `QuoteIntakeTest` calls `reset_sequence_for_test(1)` in both `SetUp()` and `TearDown()`. The `TearDown()` reset matters because a fatal `ASSERT_*` jumps straight there, skipping any reset at the bottom of the body.

## Assertion patterns

| Pattern | Rule |
| --- | --- |
| One logical assertion | A test fails for one reason. Several `EXPECT_EQ` calls are fine when they check one behavior, such as a valid shipment's `amount_cents` and `quote_id`. |
| Custom assertion helper | A recurring multi-field check becomes one function returning `::testing::AssertionResult`. |
| Guard assertion | `ASSERT_TRUE(result.has_value())` stops execution before an undefined dereference; the `EXPECT_*` calls after it carry the outcome. |
| `EXPECT_*` by default | `ASSERT_*` only where a later line would crash or be meaningless, so one regression that breaks a quote's price and id reports two failures. |

A custom assertion helper:

```cpp
::testing::AssertionResult QuoteMatches(const Quote& actual, const std::string& expected_id,
                                         Cents expected_amount) {
    if (actual.quote_id != expected_id) {
        return ::testing::AssertionFailure() << "quote_id: expected " << expected_id
                                              << ", got " << actual.quote_id;
    }
    if (actual.amount_cents != expected_amount) {
        return ::testing::AssertionFailure() << "amount_cents: expected " << expected_amount
                                              << ", got " << actual.amount_cents;
    }
    return ::testing::AssertionSuccess();
}
```

`EXPECT_TRUE(QuoteMatches(result.value(), "Q-0001", 1225))` reads as one intention, and the failure names the field that diverged.

## Test smells

| Smell | Symptom | Fix in this stack |
|---|---|---|
| **Fragile test** | A refactor that changes no behavior still breaks tests | Depend on the collaborator's interface (`Clock`, `QuoteLedger`, `CarrierGateway`), never on a concrete implementation's internals |
| **Erratic (flaky) test** | Passes or fails depending on run order or wall-clock timing | Replace `std::chrono::system_clock::now()` with an injected `Clock` double; reset process-wide state such as `SequentialQuoteIdGenerator`'s counter in both `SetUp` and `TearDown` |
| **Obscure test** | A reader cannot tell what behavior a failure indicates | One behavior per `TEST_F`, named for that behavior (a name that says a rejected shipment never touches the ledger, not `Test3`) |
| **Test code duplication** | The same ledger-and-generator wiring is retyped in every test | Fixture member declarations plus a custom assertion helper, as shown above, instead of repeating construction and field checks inline |
| **Conditional test logic** | A test body branches on `if`/`else` or loops over cases | `TEST_P` with `INSTANTIATE_TEST_SUITE_P` for a table of shipment requests, instead of a loop inside one test that can silently skip a case |
| **Slow test** | A suite that should run in milliseconds takes seconds because it touches a real store or a real carrier endpoint | A `Fake` in-memory tracking store or carrier gateway in place of the real connection; reserve the real `TrackingStoreConnection` pool (see [Resource pools]({{ '/docs/24-resource-pools/' | relative_url }})) for integration tests that exercise it directly |

[Seams]({{ '/docs/27-seams/' | relative_url }}) builds the erratic-test fix for a hidden clock call end to end: `ShipmentSlaChecker` stops calling the system clock and takes any `Clock`, which makes its deadline boundary testable at all.

## For deeper coverage

- Gerard Meszaros, [*xUnit Test Patterns: Refactoring Test Code*](https://www.informit.com/store/xunit-test-patterns-refactoring-test-code-9780132800051)
  (Addison-Wesley): the source for every pattern name in this appendix, four-phase
  test, the double taxonomy, fixture strategy, and the test smell catalog.
- Hyuk Myeong, *Googletest in Practice*, self-published: the GoogleTest and
  GoogleMock idioms this appendix maps Meszaros's vocabulary onto, read alongside
  the official [GoogleTest documentation](https://google.github.io/googletest/).

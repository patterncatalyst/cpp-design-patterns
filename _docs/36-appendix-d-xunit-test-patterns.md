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

## What this appendix covers

Gerard Meszaros's *xUnit Test Patterns* gave the testing world a shared vocabulary:
four-phase test, test double, fixture, test smell. Every example in this book that
ships a test suite already uses that vocabulary, just without naming it. This
appendix names it, and maps each term onto the GoogleTest and gMock stack the whole
book builds on, with the same collaborators you have already seen: a quote ledger,
a tracking store, a carrier gateway, a clock.

## The four-phase test

Meszaros splits every test into four phases, whether the test author labels them or
not:

1. **Setup** (arrange): build the object under test and its collaborators.
2. **Exercise** (act): call the one behavior under test.
3. **Verify** (assert): check the outcome.
4. **Teardown**: release anything setup acquired, reset anything global.

A GoogleTest fixture splits phases one and four out of the test body entirely. The
fixture's constructor (or `SetUp`) runs setup once per test; its destructor (or
`TearDown`) runs teardown once per test, even when that test fails partway through.
A `TEST_F` body then holds only exercise and verify:

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

Setting an `EXPECT_CALL` is still setup, not verify, even though it reads like an
assertion: it declares what the mock should do when called, before the call
happens. The fixture member declarations above `TEST_F` are setup too, run fresh for
every test case because GoogleTest default-constructs a new fixture instance per
test. See the full test suite in the [GoogleTest and GoogleMock chapter]({{ '/docs/07-testing-with-googletest/' | relative_url }}).

## The test-double taxonomy

Meszaros names five kinds of test double. The book's stack has a concrete answer
for each.

| Double | What it does | In this stack |
|---|---|---|
| **Dummy** | A placeholder that satisfies a signature but is never exercised | A `CarrierGateway&` passed into a constructor whose code path under test never calls it |
| **Stub** | Returns canned values on request, no verification of how it was called | A gMock `ON_CALL` on `Clock::now()`, or a hand-written `StubTrackingStore` that always returns one fixed manifest |
| **Spy** | Records what was called, inspected after the fact without a prior expectation | A hand-written `SpyCarrierGateway` that appends every `submit_label_request()` argument to a vector the test reads after exercise |
| **Mock** | Verifies expected interactions as part of the test, failing if they are not met | `gMock`'s `EXPECT_CALL`, as `MockQuoteLedger` uses on `record()` |
| **Fake** | A working, lightweight implementation that behaves correctly but is unsuited to production | An in-memory `InMemoryTrackingStore` backed by a hash map, used in place of a real store connection |

**Dummy.** The book's `QuoteIntake` constructor takes both a `QuoteLedger&` and a
`QuoteIdGenerator&`. A test that only cares about id generation can hand it a
`QuoteLedger` dummy, a minimal subclass whose `record()` returns `true` and nothing
else, because that test never inspects what the ledger received.

**Stub.** A `Clock` stub pins "now" to one fixed instant so a shipment SLA checker's
deadline math is deterministic:

```cpp
ON_CALL(clock_, now()).WillByDefault(Return(fixed_instant));
```

No `EXPECT_CALL` here, because the test does not care how many times `now()` is
called, only what it answers.

**Spy.** A hand-written `SpyCarrierGateway` records every label request it receives
instead of returning a scripted answer:

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

The test exercises the dispatcher, then asserts against `requests_seen` directly,
no `EXPECT_CALL` involved. A spy fits better than a mock when the assertion is
easier to express as "look at what accumulated" than as "declare the expectation up
front."

**Mock.** `gMock`'s `EXPECT_CALL` makes the interaction itself the assertion, as
`MockQuoteLedger` shows for `record()`, and as a `MockCarrierGateway` would for
verifying a dispatcher never retries a carrier call after a non-retryable rejection:

```cpp
EXPECT_CALL(carrier_gateway_, submit_label_request(_)).Times(1);
```

**Fake.** An `InMemoryTrackingStore` is a working implementation: it records
events, returns them back, and enforces the same ordering contract the production
store does, but keeps everything in a `std::unordered_map` instead of opening a
connection. The [connection-pool chapter]({{ '/docs/24-resource-pools/' | relative_url }}) shows the real
`TrackingStoreConnection` this fake stands in for; a fake is worth writing exactly
when a stub's "return one canned value" is too thin to exercise the logic under
test, for example a tracking query that depends on events recorded earlier in the
same test.

## Fixture strategies

**Fresh fixture.** GoogleTest's default: a new fixture instance, and therefore new
member objects, for every single `TEST_F`. `QuoteIntakeTest` above is a fresh
fixture; nothing a test does to `ledger_` or `intake_` is visible to any other test,
because no other test shares that instance.

**Shared (suite) fixture.** `SetUpTestSuite` and `TearDownTestSuite` run once for an
entire `TEST_F` suite, not once per test, and hold their state in `static` members.
A suite-level fixture is tempting when a collaborator is expensive to build, a
`CarrierGateway` wrapping a real network client being the obvious candidate, but it
reintroduces the exact problem fresh fixtures exist to avoid: state one test leaves
behind is now visible to the next test in the same suite, and test order stops
being safe to shuffle. Reach for a shared fixture only when the collaborator is
read-only and immutable for the whole suite's lifetime, such as a `CarrierRateCard`
loaded once from a fixed table.

**Transient vs. persistent fixture.** A transient fixture starts empty and is
discarded after the test, the default for `InMemoryTrackingStore` constructed fresh
per test. A persistent fixture is carried across tests or across runs by design,
for example a `TrackingStore` backed by a file the test suite seeds once and later
tests query against. A persistent fixture needs an explicit reset step, the same
discipline `SequentialQuoteIdGenerator::reset_sequence_for_test` applies in
`QuoteIntakeTest`'s `SetUp` and `TearDown` both, because a generator with
process-wide state does not reset itself just because a fresh fixture object was
constructed around it. Resetting in both `SetUp` and `TearDown` matters: a fatal
`ASSERT_*` failure mid-test jumps straight to `TearDown`, skipping whatever reset
code might have sat at the bottom of the test body instead.

## Assertion patterns

**One logical assertion per test.** A test should fail for exactly one reason. That
can still mean several `EXPECT_EQ` calls, as `AcceptsAndRecordsAValidShipment`
checks both `amount_cents` and `quote_id`, as long as every check is verifying the
same one behavior (that a valid shipment produces a correct quote) rather than
bundling two unrelated behaviors into one test.

**Custom assertion helpers.** When the same multi-field check recurs across tests,
wrap it in a function returning `::testing::AssertionResult` instead of repeating
the field list:

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

Calling `EXPECT_TRUE(QuoteMatches(result.value(), "Q-0001", 1225))` reads as one
intention and produces a message that names which field diverged, instead of a bare
`EXPECT_EQ` failure a reader has to trace back to a struct definition.

**Guard assertions.** `ASSERT_TRUE(result.has_value())` before dereferencing
`result` is a guard assertion: it exists to stop the test before undefined
behavior, not to report the interesting outcome. The interesting outcome is the
`EXPECT_EQ` calls that follow it.

**EXPECT vs. ASSERT.** `ASSERT_*` aborts the current test function on failure;
`EXPECT_*` records the failure and lets the test keep running. Use `ASSERT_*` when a
later line would crash or produce a meaningless result if the assertion failed, a
`std::optional` dereference or a pointer dereference being the common case. Use
`EXPECT_*` everywhere else, so a single regression that breaks both a quote's price
and its id gets reported as two failures instead of hiding the second behind the
first.

## Test smells

Meszaros catalogs dozens of test smells. These six show up most often in a C++
suite built on this stack, each with the fix this book's examples already apply.

| Smell | Symptom | Fix in this stack |
|---|---|---|
| **Fragile test** | A refactor that changes no behavior still breaks tests | Depend on the collaborator's interface (`Clock`, `QuoteLedger`, `CarrierGateway`), never on a concrete implementation's internals |
| **Erratic (flaky) test** | Passes or fails depending on run order or wall-clock timing | Replace `std::chrono::system_clock::now()` with an injected `Clock` double; reset process-wide state such as `SequentialQuoteIdGenerator`'s counter in both `SetUp` and `TearDown` |
| **Obscure test** | A reader cannot tell what behavior a failure indicates | One behavior per `TEST_F`, named for that behavior (`RejectsAnInvalidShipmentWithoutTouchingTheLedger`, not `Test3`) |
| **Test code duplication** | The same ledger-and-generator wiring is retyped in every test | Fixture member declarations plus a custom assertion helper, as shown above, instead of repeating construction and field checks inline |
| **Conditional test logic** | A test body branches on `if`/`else` or loops over cases | `TEST_P` with `INSTANTIATE_TEST_SUITE_P` for a table of shipment requests, instead of a loop inside one test that can silently skip a case |
| **Slow test** | A suite that should run in milliseconds takes seconds because it touches a real store or a real carrier endpoint | A `Fake` in-memory tracking store or carrier gateway in place of the real connection; reserve the real `TrackingStoreConnection` pool (see the [connection-pool chapter]({{ '/docs/24-resource-pools/' | relative_url }})) for integration tests that exercise it directly |

The [object seam chapter]({{ '/docs/27-seams/' | relative_url }})
is where the erratic-test fix for a hidden clock call gets built end to end: a
`ShipmentSlaChecker` that used to call the system clock directly becomes
constructible with any `Clock`, which is what makes its deadline boundary testable
at all, not just less flaky.

## For deeper coverage

- Gerard Meszaros, [*xUnit Test Patterns: Refactoring Test Code*](https://www.informit.com/store/xunit-test-patterns-refactoring-test-code-9780132800051)
  (Addison-Wesley): the source for every pattern name in this appendix, four-phase
  test, the double taxonomy, fixture strategy, and the test smell catalog.
- Hyuk Myeong, *Googletest in Practice*, self-published: the GoogleTest and
  GoogleMock idioms this appendix maps Meszaros's vocabulary onto, read alongside
  the official [GoogleTest documentation](https://google.github.io/googletest/).

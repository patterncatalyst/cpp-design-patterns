---
title: "Mocking with gMock"
order: 35
layout: tutorial
sectionid: docs
part: appendices
appendix: "C"
description: "A reference for gMock: defining mocks with MOCK_METHOD, setting expectations with EXPECT_CALL, matchers, cardinalities, actions, sequences, and the nice/strict spectrum, all on shipping collaborators."
duration: "15 min"
---

## What this appendix covers

Every test double in this book that stands in for a collaborator interface is
built with GoogleMock, gMock for short. It ships inside the same GoogleTest
Conan package every example already depends on, so there is no extra
dependency to add: wherever `#include <gtest/gtest.h>` already works,
`#include <gmock/gmock.h>` does too. This appendix gathers the mechanics in
one place: how to define a mock, how to set expectations on it, the matcher
and action vocabulary, and the nice/strict spectrum that controls what an
unexpected call does to a test run.

The [testing with GoogleTest chapter]({{ '/docs/07-testing-with-googletest/' | relative_url }})
and the [seams chapter]({{ '/docs/27-seams/' | relative_url }}) both build and use mocks
as part of a larger argument about design; this appendix is the reference you
return to once that design is in place and you need the exact syntax.

## When a mock is the right double

A mock verifies an interaction: that a collaborator was called, how many
times, with which arguments, and in what order relative to other calls. A
fake or a stub verifies state: it stores data and hands it back, and a test
inspects the result after the fact without caring how many calls it took to
get there.

`MockQuoteLedger` from the GoogleTest chapter is a mock because the thing
worth asserting is the call itself: does `QuoteIntake::submit()` call
`record()` exactly once, with the price it just computed, and does it skip
the call entirely when validation fails? No amount of inspecting a ledger's
final contents answers "did it get called zero times or one time with the
wrong argument." `MockClock` from the seams chapter is a mock for the same
reason: a test needs to say "the clock answers this exact instant," not
"the clock eventually reaches some time in a range."

Contrast that with a connection pool's in-memory test double, which just
needs to hand back objects and track how many it created. That is state, not
interaction, and a hand-written fake is usually a better fit. The last
section in this appendix returns to that line in more detail.

## Defining a mock

A mock is a class that inherits from the interface it stands in for and
replaces each virtual method with `MOCK_METHOD`. The modern macro form takes
the return type, the method name, the parenthesized argument list, and a
parenthesized list of specifiers:

```cpp
MOCK_METHOD(ReturnType, method_name, (ArgTypes...), (specifiers...));
```

A shipment-tracking store that a label-printing workflow writes to and reads
back from:

{% raw %}
```cpp
#include <gmock/gmock.h>

class TrackingStore {
public:
    virtual ~TrackingStore() = default;
    virtual void record_event(const std::string& shipment_id, TrackingEvent event) = 0;
    virtual std::optional<TrackingEvent> latest_event(const std::string& shipment_id) const = 0;
};

class MockTrackingStore : public TrackingStore {
public:
    MOCK_METHOD(void, record_event, (const std::string& shipment_id, TrackingEvent event), (override));
    MOCK_METHOD(std::optional<TrackingEvent>, latest_event, (const std::string& shipment_id), (const, override));
};
```
{% endraw %}

The specifiers list is where `const`, `noexcept`, and `override` go, each as
its own entry. A carrier gateway that pricing code calls for a rate quote and
later calls again to cancel a booking shows both:

{% raw %}
```cpp
class CarrierGateway {
public:
    virtual ~CarrierGateway() = default;
    virtual CarrierQuote request_quote(const ShipmentRequest& request) const = 0;
    virtual bool cancel_booking(const std::string& booking_id) noexcept = 0;
};

class MockCarrierGateway : public CarrierGateway {
public:
    MOCK_METHOD(CarrierQuote, request_quote, (const ShipmentRequest& request), (const, override));
    MOCK_METHOD(bool, cancel_booking, (const std::string& booking_id), (noexcept, override));
};
```
{% endraw %}

The parentheses around the argument list matter even for a single argument,
and even for zero arguments: `MOCK_METHOD(bool, is_connected, (), (const,
override))` needs the empty pair. Without them, the macro cannot tell where
the argument list ends and the specifier list begins, since a bare
comma-separated argument list and a bare comma-separated specifier list look
identical to the preprocessor.

A mock must override every pure virtual method its interface declares. A mock
that leaves a pure virtual un-mocked is still an abstract class, so a test cannot
construct it at all, whether or not anything calls that method. Non-pure virtual
methods are different: leave them unmocked and the mock inherits the base class's
implementation for those.

## Setting expectations

`EXPECT_CALL` is both the assertion and the stand-in behavior in one
statement. It says: this call must happen (or must not, or must happen some
number of times), and here is what it should return when it does.

```cpp
EXPECT_CALL(mock_carrier_gateway, request_quote(_))
    .WillOnce(Return(CarrierQuote{"GRD-1120", 4'850}));
```

Every `EXPECT_CALL` is verified when the mock object is destroyed, not when
the statement runs. A test that sets an expectation, never triggers it, and
then falls off the end of the test body fails with a clear message naming
the unsatisfied expectation, pointing back at the `EXPECT_CALL` line that
made the promise.

`ON_CALL` sets default behavior without making a claim about whether or how
often the call happens. It answers "if this gets called, answer with this,"
with no assertion attached:

```cpp
ON_CALL(mock_clock, now()).WillByDefault(Return(reference_instant));
```

Reach for `ON_CALL` when a mock plays a supporting role in a test and only
one or two calls are the actual subject under test. A pricing test that
exercises `CarrierGateway::request_quote()` directly still needs a `Clock`
collaborator wired in for an unrelated timestamp; `ON_CALL` gives that clock
a sane default so the test body never has to mention it, while the
`EXPECT_CALL` on `request_quote()` carries the assertion the test is
actually about. When both an `ON_CALL` and an `EXPECT_CALL` exist for the
same call, the `EXPECT_CALL`'s action wins; `ON_CALL` only fills in behavior
for calls no `EXPECT_CALL` covers.

## Argument matchers

A matcher inside `EXPECT_CALL`'s parentheses narrows which calls the
expectation applies to, and some matchers reach into the argument itself
rather than only comparing it whole.

| Matcher | Matches |
|---|---|
| `_` | Any value, ignored |
| `Eq(x)` | Equal to `x` (also the default when a literal is passed directly) |
| `Ne(x)` | Not equal to `x` |
| `Gt(x)` / `Lt(x)` | Greater than / less than `x` |
| `Field(&T::member, m)` | A struct's public data member matches `m` |
| `Property(&T::getter, m)` | A class's getter's return value matches `m` |
| `AllOf(m1, m2, ...)` | Every listed matcher matches |
| `Truly(predicate)` | A free function or lambda returns true for the argument |

`Field` is the one that catches the bug `Eq` on a whole struct cannot: a
carrier gateway call that passes the right weight but the wrong destination
zone.

{% raw %}
```cpp
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Gt;

EXPECT_CALL(mock_carrier_gateway,
            request_quote(AllOf(Field(&ShipmentRequest::weight_kg, Gt(0.0)),
                                 Field(&ShipmentRequest::zone, ServiceZone::Regional))))
    .WillOnce(Return(CarrierQuote{"GRD-1121", 6'200}));
```
{% endraw %}

`Truly` covers a check no built-in matcher expresses directly, such as a
tracking event's timestamp falling inside a business-hours window:

```cpp
EXPECT_CALL(mock_tracking_store, record_event(_, Truly(is_within_business_hours)));
```

A bare literal, like `record(_)` or `cancel_booking("BKG-9")`, is treated as
`Eq(...)` automatically. Writing `Eq` explicitly only matters when it needs
to combine with another matcher through `AllOf` or similar.

## Cardinalities

`Times(n)` states exactly how many calls an expectation covers. `Times(0)`
is the cardinality that turns a non-interaction into the assertion itself,
the same pattern `QuoteIntake`'s validation-rejection test uses to prove a
rejected request never reaches the ledger at all.

| Cardinality | Meaning |
|---|---|
| `Times(n)` | Exactly `n` calls |
| `AtLeast(n)` | `n` or more calls |
| `AtMost(n)` | Up to `n` calls |
| `AnyNumber()` | Zero or more calls, no assertion on the count |

When `Times` is omitted, gMock infers a cardinality from the actions
attached: one `WillOnce` with no `WillRepeatedly` implies exactly one call;
each additional `WillOnce` raises that implied count by one; a trailing
`WillRepeatedly` with no explicit `Times` implies `AtLeast` the number of
chained `WillOnce` calls. An `EXPECT_CALL` with no actions and no `Times` at
all implies exactly one call.

```cpp
EXPECT_CALL(mock_carrier_gateway, cancel_booking(_))
    .Times(AtLeast(1))
    .WillRepeatedly(Return(true));
```

## Actions

An action is what a mocked call does once it matches: compute or return a
value, run arbitrary code, or some combination.

| Action | Effect |
|---|---|
| `Return(value)` | Returns `value` from the call |
| `ReturnRef(variable)` | Returns a reference to `variable`, for methods that return a reference |
| `Invoke(callable)` | Forwards the call's arguments to `callable` and returns its result |
| `DoAll(a1, a2, ...)` | Runs every listed action in order; only the last one's return value is used |

`WillOnce` attaches an action to one matching call and is consumed after it
fires; a second matching call falls through to the next `WillOnce` in the
chain, or to `WillRepeatedly` if one follows. `WillRepeatedly` attaches an
action to every matching call that is left once the `WillOnce` chain, if
any, is exhausted.

```cpp
EXPECT_CALL(mock_clock, now())
    .WillOnce(Return(dispatch_instant))
    .WillOnce(Return(dispatch_instant + std::chrono::hours(2)))
    .WillRepeatedly(Return(dispatch_instant + std::chrono::hours(4)));
```

The first call to `now()` returns the dispatch instant, the second returns
two hours later, and every call after that returns four hours later. A
chain like this lets one test exercise an ETA estimator across several
readings of the same `Clock` seam the SLA checker uses, without threading a
real clock through the test or sleeping.

`DoAll` combines a side effect with a return value, such as recording that a
call happened in a local counter while still answering with a canned
result:

```cpp
EXPECT_CALL(mock_tracking_store, latest_event("SHM-4471"))
    .WillOnce(DoAll(InvokeWithoutArgs([&] { lookups += 1; }),
                     Return(TrackingEvent{"SHM-4471", Status::InTransit})));
```

`Invoke` is the escape hatch for behavior too specific for a canned
`Return`, such as a carrier gateway stub that computes its quote from the
actual request instead of a fixed number:

```cpp
EXPECT_CALL(mock_carrier_gateway, request_quote(_))
    .WillOnce(Invoke([](const ShipmentRequest& request) {
        return CarrierQuote{"GRD-1122", static_cast<int>(request.weight_kg * 140)};
    }));
```

## Ordered expectations

By default, gMock does not care about the order calls arrive in across
different `EXPECT_CALL` statements; it only checks that each one is
satisfied by the time the mock is destroyed. `InSequence` adds that
constraint when the order itself is part of the contract, such as a booking
workflow that must request a quote before it ever cancels one:

```cpp
{
    ::testing::InSequence seq;
    EXPECT_CALL(mock_carrier_gateway, request_quote(_)).WillOnce(Return(quote));
    EXPECT_CALL(mock_carrier_gateway, cancel_booking(quote.booking_id)).WillOnce(Return(true));
}
```

Every `EXPECT_CALL` inside the `InSequence` object's scope joins one chain,
each depending on the one before it. `.After(other_expectation)` expresses
the same dependency one expectation at a time, which is the better fit when
only one or two calls in a larger set have a real ordering requirement and
the rest do not:

```cpp
auto& quote_call = EXPECT_CALL(mock_carrier_gateway, request_quote(_)).WillOnce(Return(quote));
EXPECT_CALL(mock_carrier_gateway, cancel_booking(_)).After(quote_call).WillOnce(Return(true));
```

## The nice/strict spectrum

An unexpected call, one that matches no `EXPECT_CALL`, is still answered
with a default value so the test keeps running, but the three mock base
behaviors disagree on what else happens.

| Wrapper | An unexpected call |
|---|---|
| Plain mock (`MockCarrierGateway`) | Logs a warning, the default |
| `NiceMock<MockCarrierGateway>` | Stays silent |
| `StrictMock<MockCarrierGateway>` | Fails the test |

A plain mock is called naggy for exactly this reason: every call a test
never bothered to expect prints a warning to the test log, which is useful
while a test is being written and noisy once a suite has dozens of mocks
with a handful of incidental calls apiece. `NiceMock` quiets a collaborator
a test only cares about for one or two calls, such as a `Clock` that a
pricing test wires in purely so a timestamp field is not left default-
constructed. `StrictMock` goes the other way: wrap `MockTrackingStore` in
`StrictMock` when a test's whole point is that a particular code path must
never write to the tracking store at all, and any other call, even one that
would otherwise go unnoticed, should fail loudly.

```cpp
::testing::StrictMock<MockTrackingStore> tracking_store;
EXPECT_CALL(tracking_store, record_event("SHM-4471", _)).Times(1);
```

## Mock vs fake: when to hand-write a fake

A mock is the right double when the test's assertion is about the call itself.
A fake is the right double when the test's assertion is about the resulting
state, and the collaborator's own logic, not just its call count, needs to
behave consistently across several calls in the same test.

The resource pool chapter's examples lean on hand-written fakes for exactly
this reason: a test double connection factory that hands back distinct
connection objects and tracks how many it has created so far is state a
fake tracks naturally in a member variable, not an interaction a mock needs
to assert on call by call. Writing that same behavior as a mock would mean
chaining a `WillOnce` for every connection the test ever acquires, which
turns a two-line fake into a maintenance burden that grows with the test.

The dividing line is usually this: if the next sentence after "this test
checks that..." names a method and an argument, reach for a mock. If it
names a value the collaborator should be holding afterward, reach for a
fake.

## For deeper coverage

- [gMock for Dummies](https://google.github.io/googletest/gmock_for_dummies.html), the official
  introduction to the matcher, action, and expectation vocabulary this appendix summarizes.
- [gMock Cookbook](https://google.github.io/googletest/gmock_cook_book.html), the official
  reference for less common recipes: matching on multiple arguments at once, custom
  matchers, and mocking non-virtual interfaces through templates.
- Hyuk Myeong, *Googletest in Practice*, self-published: the GoogleTest and GoogleMock
  idioms every mock in this book is verified against.

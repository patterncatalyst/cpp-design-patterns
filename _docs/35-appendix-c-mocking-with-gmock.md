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

## Learning objectives

By the end of this appendix you can:

- Choose between a mock and a hand-written fake for a collaborator.
- Define a mock with `MOCK_METHOD`, including `const` and `noexcept` methods.
- Constrain a call with matchers, cardinalities, actions and ordering.
- Pick a plain, `NiceMock` or `StrictMock` wrapper.

## Why this matters

gMock ships in the GoogleTest Conan package every example already uses: wherever `#include <gtest/gtest.h>` works, `#include <gmock/gmock.h>` does too. [Testing with GoogleTest]({{ '/docs/07-testing-with-googletest/' | relative_url }}) and [Seams]({{ '/docs/27-seams/' | relative_url }}) use mocks inside a design argument; this page is the syntax reference.

## Mock or fake

A mock verifies an interaction: that a collaborator was called, how many times, with which arguments, in what order. A fake verifies state: it stores data and hands it back, and you read the result afterward.

| Double | Assert on | Example |
| --- | --- | --- |
| Mock | The call itself | `MockQuoteLedger`: `QuoteIntake::submit()` calls `record()` once with the computed price, and not at all when validation fails |
| Mock | A scripted answer | `MockClock`: the clock returns this exact instant |
| Fake | State after several calls | A resource pool's stand-in resource that counts its own constructions |

Inspecting a ledger's final contents can't tell zero calls from one call with the wrong argument; that needs a mock. A resource that counts its constructions is state a member variable tracks naturally. As a mock it would need a `WillOnce` per acquisition, a chain that grows with the test.

The rule of thumb: if "this test checks that..." continues with a method and an argument, use a mock. If it continues with a value the collaborator holds afterward, use a fake.

## Defining a mock

A mock inherits from the interface and replaces each virtual method with `MOCK_METHOD`: return type, name, parenthesized argument list, parenthesized specifiers.

```cpp
MOCK_METHOD(ReturnType, method_name, (ArgTypes...), (specifiers...));
```

A tracking store that a label-printing workflow writes to and reads back:

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

`const`, `noexcept` and `override` each go as their own entry in the specifier list. A carrier gateway shows both:

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

| Rule | Why |
| --- | --- |
| Parenthesize the argument list even for one or zero arguments: `MOCK_METHOD(bool, is_connected, (), (const, override))` | The preprocessor can't otherwise tell where the arguments end and the specifiers begin. |
| Mock every pure virtual method | A mock with an unmocked pure virtual is still abstract and can't be constructed. |
| Non-pure virtuals may stay unmocked | The mock inherits the base implementation. |

## EXPECT_CALL and ON_CALL

`EXPECT_CALL` is the assertion and the stand-in behavior in one statement: this call must happen, and here's what it returns.

```cpp
EXPECT_CALL(mock_carrier_gateway, request_quote(_))
    .WillOnce(Return(CarrierQuote{"GRD-1120", 4'850}));
```

gMock verifies every `EXPECT_CALL` when the mock is destroyed, not when the statement runs. An expectation that's never triggered fails at the end of the test with a message pointing at the `EXPECT_CALL` line.

`ON_CALL` sets a default with no claim about whether or how often the call happens:

```cpp
ON_CALL(mock_clock, now()).WillByDefault(Return(reference_instant));
```

Use `ON_CALL` for a collaborator in a supporting role, such as a `Clock` a pricing test needs only for an unrelated timestamp, and keep `EXPECT_CALL` for the call under test. When both cover the same call, the `EXPECT_CALL` action wins.

## Argument matchers

A matcher inside `EXPECT_CALL`'s parentheses narrows which calls the expectation covers.

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

`Field` catches what `Eq` on the whole struct reports poorly: the right weight with the wrong destination zone.

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

`Truly` covers a check no built-in matcher expresses, such as a timestamp inside business hours:

```cpp
EXPECT_CALL(mock_tracking_store, record_event(_, Truly(is_within_business_hours)));
```

A bare literal, as in `cancel_booking("BKG-9")`, is `Eq(...)`. Write `Eq` explicitly only to combine it through `AllOf` or similar.

## Cardinalities

| Cardinality | Meaning |
|---|---|
| `Times(n)` | Exactly `n` calls |
| `AtLeast(n)` | `n` or more calls |
| `AtMost(n)` | Up to `n` calls |
| `AnyNumber()` | Zero or more calls, no assertion on the count |

`Times(0)` makes a non-interaction the assertion: a rejected `QuoteIntake` request never reaches the ledger.

Without `Times`, gMock infers the count from the actions:

| Actions attached | Implied cardinality |
| --- | --- |
| None | Exactly one call |
| `n` × `WillOnce` | Exactly `n` calls |
| `n` × `WillOnce`, then `WillRepeatedly` | `AtLeast(n)` |

```cpp
EXPECT_CALL(mock_carrier_gateway, cancel_booking(_))
    .Times(AtLeast(1))
    .WillRepeatedly(Return(true));
```

## Actions

| Action | Effect |
|---|---|
| `Return(value)` | Returns `value` from the call |
| `ReturnRef(variable)` | Returns a reference to `variable`, for methods that return a reference |
| `Invoke(callable)` | Forwards the call's arguments to `callable` and returns its result |
| `DoAll(a1, a2, ...)` | Runs every listed action in order; only the last one's return value is used |

Each `WillOnce` serves one matching call and is consumed. Later calls fall through to the next `WillOnce`, then to `WillRepeatedly`, which serves every call left over.

```cpp
EXPECT_CALL(mock_clock, now())
    .WillOnce(Return(dispatch_instant))
    .WillOnce(Return(dispatch_instant + std::chrono::hours(2)))
    .WillRepeatedly(Return(dispatch_instant + std::chrono::hours(4)));
```

The first `now()` returns the dispatch instant, the second two hours later, and every later call four hours later. An ETA estimator can read the `Clock` seam several times with no real clock and no sleep.

`DoAll` pairs a side effect with a return value:

```cpp
EXPECT_CALL(mock_tracking_store, latest_event("SHM-4471"))
    .WillOnce(DoAll(InvokeWithoutArgs([&] { lookups += 1; }),
                     Return(TrackingEvent{"SHM-4471", Status::InTransit})));
```

`Invoke` computes the answer from the actual arguments when a canned `Return` won't do:

```cpp
EXPECT_CALL(mock_carrier_gateway, request_quote(_))
    .WillOnce(Invoke([](const ShipmentRequest& request) {
        return CarrierQuote{"GRD-1122", static_cast<int>(request.weight_kg * 140)};
    }));
```

## Ordered expectations

By default gMock ignores the order of calls across different `EXPECT_CALL` statements. `InSequence` makes order part of the contract, such as a booking workflow that must request a quote before it cancels:

```cpp
{
    ::testing::InSequence seq;
    EXPECT_CALL(mock_carrier_gateway, request_quote(_)).WillOnce(Return(quote));
    EXPECT_CALL(mock_carrier_gateway, cancel_booking(quote.booking_id)).WillOnce(Return(true));
}
```

Every `EXPECT_CALL` in the `InSequence` scope joins one chain. `.After(other)` states one dependency at a time, which fits when only one or two calls in a larger set have an order:

```cpp
auto& quote_call = EXPECT_CALL(mock_carrier_gateway, request_quote(_)).WillOnce(Return(quote));
EXPECT_CALL(mock_carrier_gateway, cancel_booking(_)).After(quote_call).WillOnce(Return(true));
```

## The nice/strict spectrum

An uninteresting call, to a method with no `EXPECT_CALL` at all, returns a default value and execution continues. The wrapper decides what else happens.

| Wrapper | An uninteresting call |
|---|---|
| Plain mock (`MockCarrierGateway`) | Logs a warning, the default |
| `NiceMock<MockCarrierGateway>` | Stays silent |
| `StrictMock<MockCarrierGateway>` | Fails the test |

A call that does have an `EXPECT_CALL` but matches none of them is a failure under every wrapper.

| Use | When |
| --- | --- |
| Plain ("naggy") | While writing a test; the warnings show calls you haven't decided about. |
| `NiceMock` | A supporting collaborator, such as a `Clock` wired in only so a timestamp isn't default-constructed. |
| `StrictMock` | A code path must make no call beyond the expected ones, such as touching `MockTrackingStore` only once. |

```cpp
::testing::StrictMock<MockTrackingStore> tracking_store;
EXPECT_CALL(tracking_store, record_event("SHM-4471", _)).Times(1);
```

## For deeper coverage

- [gMock for Dummies](https://google.github.io/googletest/gmock_for_dummies.html), the official
  introduction to the matcher, action, and expectation vocabulary this appendix summarizes.
- [gMock Cookbook](https://google.github.io/googletest/gmock_cook_book.html), the official
  reference for less common recipes: matching on multiple arguments at once, custom
  matchers, and mocking non-virtual interfaces through templates.
- Hyuk Myeong, *Googletest in Practice*, self-published: the GoogleTest and GoogleMock
  idioms every mock follows.

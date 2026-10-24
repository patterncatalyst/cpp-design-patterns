---
layout: example
order: 7
title: "Testing with GoogleTest"
description: A freight quote-intake component exercised through all five GoogleTest and GoogleMock styles, discovered by CTest.
github_path: examples/07-testing-with-gtest
card_eyebrow: "Chapter 7 · Testing"
card_title: "Quote intake: GoogleTest styles"
card_blurb: "Fixtures, parameterized tests, typed tests, GoogleMock interaction tests, and a death test, each used where it genuinely fits, all discovered and run through CTest."
---

A freight carrier's quote-intake component used to showcase every GoogleTest
and GoogleMock style, each applied where it actually fits:

- **Fixture (`TEST_F`)** with `SetUp`/`TearDown` resetting shared state.
- **Parameterized (`TEST_P` + `INSTANTIATE_TEST_SUITE_P`)** over a validation
  table, with readable case names.
- **Typed (`TYPED_TEST_SUITE`)** proving one id-generator contract against two
  unrelated concrete types.
- **GoogleMock interaction tests** with `MOCK_METHOD`, `EXPECT_CALL`, matchers,
  and actions, including proving the ledger is never touched on a rejected
  shipment.
- **Death test (`EXPECT_DEATH`)** on a precondition, using a custom macro that
  survives the Release `-DNDEBUG` build.

The chapter also contrasts `ASSERT_*` versus `EXPECT_*` and explains when each
style is the wrong tool.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 15/15 tests pass
./examples/07-testing-with-gtest/demo.sh cpp23
```

`ctest` discovers every case (`gtest_discover_tests`), and the demo runs the
quote-intake flow end to end. Chapter 7 walks through each test style and when
to reach for it. Builds standalone and under the `cpp20` preset (15 tests).

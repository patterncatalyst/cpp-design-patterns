---
layout: example
order: 27
title: "Seams and the test safety net"
description: Getting inherited C++ under test before changing it, taught through a shipment-SLA checker hard-wired to the wall clock.
github_path: examples/27-seams
card_eyebrow: "Chapter 27 · Seams"
card_title: "Pin behavior, then change it"
card_blurb: "Characterization tests pin a legacy SLA checker's current behavior, an object seam makes its clock substitutable so a boundary bug can be pinned and fixed under test, and a link seam breaks a free function's dependency on a grace-period policy."
---

Getting inherited shipping code under test on a shipment-SLA domain:

- **Characterization tests.** Pin the legacy checker's present behavior,
  including a boundary the hard-wired `std::chrono::system_clock::now()`
  dependency makes impossible to assert exactly.
- **Object seam (lead).** Extract a `Clock` interface and inject it, so a
  `MockClock` drives the checker to a chosen instant, the boundary is pinned to
  the nanosecond, and a deliberate `>` to `>=` fix is made with the test green.
- **Link seam.** Break a free function's dependency on an enterprise
  grace-period policy by selecting the implementation at link time, with no
  object to inject into. The test binary links a zero-minute double; the demo
  links the production 15-minute policy.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 11/11 tests pass
./examples/27-seams/demo.sh cpp23
```

The demo runs the legacy and seamed checkers side by side, then shows the late
fee schedule under the production grace policy. The `try_time_remaining` return
uses `std::expected` on C++23 to carry how far past the deadline a shipment is,
and `std::optional` on C++20. Builds standalone and under the `cpp20` preset
(11 tests).

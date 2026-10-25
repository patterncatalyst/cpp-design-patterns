---
layout: example
order: 28
title: "The 24 dependency-breaking techniques for C++20/23"
description: Feathers' dependency-breaking techniques modernized for C++20/23 and demonstrated on inherited shipping code.
github_path: examples/28-dependency-breaking
card_eyebrow: "Chapter 28 · Dependency breaking"
card_title: "Making the untestable testable"
card_blurb: "A representative subset of Feathers' 24 techniques on inherited shipping code: extract interface and parameterize constructor, extract and override call, encapsulate global references, introduce instance delegator, and adapt parameter."
---

Six of Feathers' dependency-breaking techniques, each on inherited shipping code:

- **Extract Interface + Parameterize Constructor.** A mail dependency pulled
  behind an `AlertChannel` interface and opened for injection.
- **Extract and Override Call.** A wall-clock read isolated into an overridable
  hook, a `virtual` in C++20 and a deducing-this hook in C++23.
- **Encapsulate Global References.** A mutable global wrapped behind an accessor
  a test can control.
- **Introduce Instance Delegator.** A static-only legacy lookup given an
  instance delegator so callers can be tested against a port.
- **Adapt Parameter.** A heavyweight file type adapted down to the two values a
  caller actually reads.

The chapter carries the full reference table of all 24 techniques mapped to the
C++20/23 tool that expresses each.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 14/14 tests pass
./examples/28-dependency-breaking/demo.sh cpp23
```

The demo runs each technique's before-and-after and shows a test double driving
the once hard-wired path. The deducing-this override is guarded for C++23;
C++20 uses the `virtual` form. Builds standalone and under the `cpp20` preset
(13 tests).

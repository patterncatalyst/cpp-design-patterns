---
layout: example
order: 29
title: "Fowler and Feathers: contrasted and combined"
description: One inherited shipping ETA estimator taken through the combined workflow, Feathers first to build a safety net, then Fowler's named refactorings.
github_path: examples/29-refactoring-workflow
card_eyebrow: "Chapter 29 · Fowler and Feathers"
card_title: "Safety net first, then refactor"
card_blurb: "An inherited ETA estimator run through both methodologies in sequence: characterize and break its clock dependency (Feathers), then apply Extract Function, Replace Conditional with Polymorphism, and Introduce Parameter Object (Fowler), tests green at every step."
---

One inherited shipping ETA estimator, taken through the full combined sequence:

- **Stage 1, Feathers.** A characterization test pins the legacy estimator's
  current output, then Extract Interface plus Parameterize Constructor break its
  hard-wired `std::chrono::system_clock::now()` dependency so a `FixedClock`
  pins the previously unpinnable holiday-adjusted dates exactly.
- **Stage 2, Fowler.** With the safety net in place, named refactorings follow:
  Extract Function names the holiday and surcharge rules, Replace Conditional
  with Polymorphism turns the carrier if/else chain into a `CarrierProfile`
  hierarchy, and Introduce Parameter Object replaces a six-argument call.

The test suite stays green at every step, and an extensibility test adds a new
carrier profile without editing the existing three.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 21/21 tests pass
./examples/29-refactoring-workflow/demo.sh cpp23
```

The demo replays the three stages and prints the pinned ETA output, identical
across C++20 and C++23. The chapter carries the Fowler-versus-Feathers contrast
table and the combined-workflow walkthrough. Builds standalone and under the
`cpp20` preset (21 tests).

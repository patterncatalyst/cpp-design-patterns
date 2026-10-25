---
layout: example
order: 32
title: "Working with AI and LLM tools"
description: A usage-tier Strategy pairing a correct implementation against an AI-plausible one with a single boundary bug, caught by the same behavior table run against both.
github_path: examples/32-ai-and-llm-tools
card_eyebrow: "Chapter 32 · AI and LLM tools"
card_title: "Gate AI output on tests"
card_blurb: "A correct usage-tier Strategy beside an AI-plausible one carrying a single off-by-one at a billing-tier boundary. The identical behavior table passes on the correct implementation and catches the wrong one, showing why AI drafts are gated on tests, not trusted on read."
---

The verify-AI-output discipline, made runnable on a usage-tier billing domain:

- **A correct Strategy** with spec-exact tier boundaries (inclusive lower bounds).
- **An AI-plausible Strategy** that looks right but carries one realistic bug: a
  `<=` where the spec wants `<` at the Starter/Growth boundary.
- **One behavior table** run against both. It passes on the correct
  implementation and catches the plausible-wrong one at the boundary value, using
  GoogleTest's `EXPECT_NONFATAL_FAILURE` so the wrong implementation is exercised
  and caught while the suite stays green.

The chapter covers prompting with the forces vocabulary, the failure modes of an
unchecked LLM, and the test-and-idiom gate that decides whether a draft ships.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 6/6 tests pass
./examples/32-ai-and-llm-tools/demo.sh cpp23
```

The demo runs both strategies over the tier table and isolates the exact dollar
figure the boundary bug produces, the kind of defect a careless read of
plausible AI output would miss. Builds standalone and under the `cpp20` preset
(6 tests).

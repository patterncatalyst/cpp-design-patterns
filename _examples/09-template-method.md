---
layout: example
order: 9
title: "Template Method and NVI"
description: An order-export pipeline with a fixed skeleton and overridable hooks, plus a CRTP vs deducing-this static-dispatch contrast.
github_path: examples/09-template-method
card_eyebrow: "Chapter 9 · Template Method"
card_title: "Order export: Template Method + NVI"
card_blurb: "A fixed export skeleton calls private virtual hooks in a set order; CSV, JSON, and EDI exporters override the hooks without touching the sequence or its invariants."
---

A warehouse order-export pipeline that shows Template Method and the
non-virtual interface idiom:

- **`OrderExportPipeline::export_order()`** is a public, non-virtual skeleton
  (validate, header, one line per item, footer, trailer) that calls four
  **private** virtual hooks. The NVI idiom keeps the public surface
  non-virtual and the customization points private.
- **`validate()` and `trailer_for()`** are non-virtual invariants that run for
  every exporter and cannot be overridden.
- Three concrete exporters (`CsvManifestExporter`, `JsonManifestExporter`,
  `FixedWidthEdiExporter`) fill the hooks differently without touching the
  sequence.

A header-only static-dispatch file contrasts C++20 CRTP against the C++23
`deducing this` form of the same compile-time hook.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 10/10 tests pass
./examples/09-template-method/demo.sh cpp23
```

The demo exports one order three ways and shows the skeleton calling the hooks
in a fixed order with the invariants always running. Chapter 9 contrasts this
with Strategy and explains when inheritance beats composition here. Builds
standalone and under the `cpp20` preset (8 tests, with the deducing-this cases
compiled out).

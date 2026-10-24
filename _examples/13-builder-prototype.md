---
layout: example
order: 13
title: "Builder, Prototype, and serialization"
description: A freight manifest assembled with a validating builder, cloned from prototypes, and round-tripped through text.
github_path: examples/13-builder-prototype
card_eyebrow: "Chapter 13 · Builder / Prototype"
card_title: "Freight manifest: build, clone, serialize"
card_blurb: "A fluent builder that validates once at build(), a prototype registry that clones named package templates into independent copies, and a text serialization round trip."
---

A freight shipment manifest assembled three ways that cooperate:

- **Builder.** `ManifestBuilder` has fluent chained setters, defaults the
  optional fields, and validates once at `build()`, throwing and naming the
  missing field on an incomplete manifest.
- **Prototype.** `PackageTemplateRegistry` holds named prototypes
  (`fragile-electronics`, `bulk-dry-goods`, `refrigerated-perishable`); `clone()`
  is a true deep copy because every member is a value type.
- **Serialization.** A plain `KEY=VALUE` + `PACKAGE ...` text format; serialize
  then deserialize reproduces a structurally equal manifest.

The C++23 codetabs path contrasts `std::format` (both standards) with
`std::println` (C++23), guarded so the C++20 build still works.

## Build, run, observe

```sh
./scripts/build.sh cpp23
./scripts/verify.sh cpp23      # 19/19 tests pass
./examples/13-builder-prototype/demo.sh cpp23
```

The demo builds a manifest, rejects an incomplete one, clones a prototype and
mutates the clone without touching the original, and round-trips a manifest
through text. Chapter 13 covers when cloning beats construction. Builds
standalone and under the `cpp20` preset (17 tests).

# Design Patterns with C++

Gang of Four design patterns, the SOLID principles, and the C++ idioms that
express them, taught as decisions under forces with a runnable, GoogleTest-backed
example for every pattern in modern C++20 and C++23.

**Live site:** https://patterncatalyst.github.io/cpp-design-patterns/

This is the first book in a planned series; Java, .NET/C#, and Python editions
follow. It is part of the [Pattern Catalyst](https://github.com/patterncatalyst)
collection.

## What this is

A chapter-based book and tutorial site for new and intermediate C++ engineers who
want to recognize what to build and why, not memorize UML diagrams. Each pattern
is shown as a response to a set of forces, implemented idiomatically in modern
C++, tested, and paired with a note on when not to use it. The book also covers
C++-specific idioms (RAII, CRTP and policy design, type erasure, PImpl,
expression templates), state machines including probability-driven ones,
patterns as they appear in production, a legacy-code modernization track, and a
chapter on working with AI and LLM tools.

Standards are shown side by side in **C++20 and C++23**. C++26 is deferred until
its headline features stabilize.

## Prerequisites

Local tools only, no containers. The reference platform is Fedora 44.

| Tool | Version |
| --- | --- |
| Clang | 22 (or GCC 16) |
| CMake | 3.28 or newer |
| Ninja | recent |
| Conan | 2.x |
| GoogleTest | 1.17.0 (fetched by Conan) |

Run `scripts/check-prereqs.sh` to verify the toolchain. Chapter 0 covers setup in
full, including CLion and VS Code.

## Build and test

Every example is standalone-buildable and also part of a root superbuild:

```sh
./scripts/build.sh cpp23      # conan install + cmake --preset + build
./scripts/verify.sh cpp23     # ctest
./scripts/build.sh cpp20      # the C++20 compatibility preset
./scripts/verify.sh cpp20
```

A single example on its own:

```sh
cd examples/08-strategy
conan install . --output-folder=build/cpp23 --build=missing -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23 && cmake --build --preset cpp23 && ctest --preset cpp23
```

## Layout

| Path | Contents |
| --- | --- |
| `_docs/` | Chapters (`NN-*.md`) |
| `_parts/` | Part landing pages |
| `examples/` | One runnable gtest example per hands-on chapter |
| `diagrams/` | Paired `.svg` + `.excalidraw` figures |
| `_plans/` | Build plans and verification status (not published) |

## Reading the site locally

```sh
bundle install
bundle exec jekyll serve
```

## Status

Parts 0 through 8 are complete (chapters 0 to 23): the SOLID principles, all 23
GoF patterns, the C++ substrate, and the C++ idiom track. Still to come: patterns
in production, the legacy-code modernization track, the final project and AI
chapter, the appendices, and the companion decks.

## License

This project is dual-licensed to separate the code from the writing:

- **Code** — everything under `examples/`, plus build and helper scripts and
  inline code snippets — is licensed under the **Apache License 2.0**
  ([`LICENSE`](LICENSE)).
- **Content** — the tutorial and book prose (chapters under `_docs/`),
  diagrams and images under `assets/`, and the slide decks under
  `presentation/` — is licensed under **Creative Commons Attribution 4.0
  International (CC BY 4.0)** ([`LICENSE-docs.md`](LICENSE-docs.md)).

Copyright 2026 PatternCatalyst (Robert Sedor).


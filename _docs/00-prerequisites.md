---
title: "Prerequisites and the toolchain"
order: 0
part: "orientation"
description: "The clang, gcc, CMake, Ninja, and Conan toolchain this book builds against on Fedora 44, the cpp23/cpp20 presets, the codetabs convention, and how every example is built, run, and tested."
duration: "20 min"
---

## Learning objectives

By the end of this chapter you can:

- Install and verify the exact local toolchain every example in this book builds with: clang, CMake, Ninja, and Conan 2.x, on Fedora 44.
- Run the one-time `conan profile detect` step and explain what it records.
- Build and test any example with `scripts/build.sh` and `scripts/verify.sh`, under both the `cpp23` and `cpp20` presets.
- Open this repository in CLion or VS Code and have either IDE drive the same presets the command line uses.
- Read a codetabs block and explain why this book shows C++20 and C++23 side by side, and why C++26 is not a third tab yet.

## Why this matters

Every pattern chapter in this book hands you real, compiling, tested C++. That promise is only as good as the toolchain behind it, so this chapter makes the toolchain real on your machine before a single pattern shows up. There is no container here to paper over a missing compiler or a stale CMake. You install clang, CMake, Ninja, and Conan directly on Fedora 44, point Conan at a profile that describes your machine once, and from there every example in the book builds the same way: `conan install`, then `cmake --preset`, then `cmake --build`, then `ctest`. Once that loop works for you, it works for every chapter that follows, in either IDE you use or from a bare terminal, and you never have to think about the build system again until Appendix A revisits it for C++26.

## Why local tools, no containers

A container is a fine way to pin a toolchain, but it also hides exactly the thing this book wants you to see: what clang, CMake, Conan, and GoogleTest actually do on a real machine, and how they hand off to each other. Design patterns live at the level of source files, compilation units, and link steps; a reader who only ever typed `docker compose up` never watches a `CMakePresets.json` pick a generator, never watches Conan resolve a dependency graph, and never develops the instinct for reading a failed CMake configure step. This book's reference platform is Fedora 44 with the toolchain installed straight onto the host. CI builds in a `fedora:44` container purely because that gives continuous integration a clean, reproducible userspace to install the same toolchain into on every run; that container is CI plumbing, not something this book ever asks you to run on your own machine.

## The stack, tool by tool

Five tools do all the work, and each one has exactly one job.

**clang** is this book's default compiler. Version 22 is the first clang release where `std::print`, `std::expected`, and the rest of the C++23 standard library surface this book relies on are solid enough to teach from without caveats, confirmed against this exact toolchain. Every `CMakePresets.json` preset in this repository pins `CMAKE_CXX_COMPILER` to `clang++` explicitly, so the compiler a reader gets is the one every example was written and tested against.

**GCC** is a supported alternate, not the default. GCC 16 compiles everything in this book correctly, but the shipped presets point at clang because that is the single compiler this book's own examples are verified against release to release. If you want to build under GCC instead, override the compiler on the command line when you configure: `cmake --preset cpp23 -DCMAKE_CXX_COMPILER=g++`. Nothing else in the build changes.

**CMake** describes how to build each example: which files compile into which target, which standard applies, which test runner picks up the result. This book needs CMake 4.x or newer, floored at the `cmake_minimum_required(3.28)` every example's `CMakeLists.txt` declares, because `CMakePresets.json` version 6 and the `gtest_discover_tests` CTest integration both depend on CMake features younger than that.

**Ninja** is the build generator CMake hands its compiled graph to. It is faster than the Unix Makefiles generator on incremental rebuilds, which matters once you are rebuilding one example after a one-line change dozens of times while working through a chapter, and every preset in this repository names it explicitly as the generator.

**Conan** is the one piece that might be unfamiliar even to an experienced C++ developer, because C++ did not have a widely adopted package manager for most of its history. Conan 2.x resolves this book's one third-party dependency, GoogleTest, downloads or builds the exact pinned version, and writes a CMake toolchain file that `cmake --preset` consumes automatically. Without it, building GoogleTest from source and wiring it into every example's `CMakeLists.txt` by hand would be its own small project; with it, `conan install` is one command and the version is pinned exactly in `conanfile.py` for every reader.

## Installing on Fedora 44

Four of the five tools come straight from Fedora's own repositories:

```bash
sudo dnf install -y clang lld gcc-c++ cmake ninja-build python3-pip
```

`lld` is clang's companion linker and worth installing alongside it even though the presets do not force it as the default; `gcc-c++` gives you the supported alternate compiler described above without a second round trip to `dnf` later.

Fedora 44 does package Conan too (`sudo dnf install conan` pulls 2.25.2 at the time of writing), and that package works fine for this book's floor version. This book installs Conan with `pip` instead, because `pip` tracks upstream Conan's own release cadence rather than waiting on a distribution's package rebuild cycle, and a newer Conan inside the same major version never changes how this book's `conanfile.py` behaves:

```bash
pip install --user --upgrade conan
```

Confirm `~/.local/bin` is on your `PATH` (most Fedora installs already add it for a user's own shell), then check what landed:

```bash
conan --version
# Conan version 2.33.0
```

Any Conan 2.x release works. This book's own verified toolchain table pins 2.25.2 as the floor; nothing later in the 2.x line changes the commands you run.

## The one-time step: `conan profile detect`

Before Conan can resolve anything, it needs a profile describing the machine it is running on: operating system, compiler, compiler version, architecture, and default build type. Generate one once, the first time you work in this repository:

```bash
conan profile detect
```

Conan inspects your installed compiler (clang, on a default Fedora 44 setup) and writes the result to `~/.conan2/profiles/default`. Every `conan install` this book ever asks you to run reads that file implicitly; you do not pass compiler flags to Conan by hand. If you ever reinstall your compiler or switch Fedora releases, rerun this command (with `--force` to overwrite an existing profile) rather than hand-editing the generated file.

## The build loop every example follows

Every example in this book, from the orientation chapters through the final project, builds and tests the exact same way. Two scripts in `scripts/` wrap the four real steps so you never have to remember Conan's `--output-folder` convention or CTest's preset flag by hand:

```bash
./scripts/build.sh cpp23    # conan install -> cmake --preset cpp23 -> cmake --build
./scripts/verify.sh cpp23   # ctest --preset cpp23 --output-on-failure
```

Underneath, `build.sh` runs `conan install . --output-folder=build/cpp23 --build=missing -s compiler.cppstd=23`, which resolves GoogleTest 1.17.0 and writes a Conan-generated CMake toolchain file into `build/cpp23/generators/`. `cmake --preset cpp23` then configures against that exact toolchain file, which `CMakePresets.json` already points at, using Ninja as the generator. `cmake --build --preset cpp23` compiles every example the root `CMakeLists.txt` superbuild discovers under `examples/`. `verify.sh` runs `ctest --preset cpp23`, which uses `gtest_discover_tests` to find every `TEST`/`TEST_F` case each example registered at configure time and reports pass or fail per case, not just per binary.

Nothing here is specific to one pattern or one chapter. Any example you reach later in the book builds this same way, either through the root superbuild or standalone from inside its own `examples/NN-name/` directory, because every example's `CMakeLists.txt` is written to configure cleanly on its own as well as when the superbuild pulls it in.

## The two presets

`CMakePresets.json` defines exactly two configure presets, matching the two standards this book teaches:

| Preset | Standard | Role |
|---|---|---|
| `cpp23` | C++23 | Default. Every chapter's primary code sample targets this standard first. |
| `cpp20` | C++20 | Compatibility. The same example, built and tested against the older standard a reader's current codebase might still require. |

Both presets share one `binaryDir` convention, `build/<preset>/`, so building both side by side never collides, and both are wired into `scripts/build.sh` and `scripts/verify.sh` as the first argument. There is no `cpp26` preset. Clang 22 accepts `-std=c++2c` at the compiler-flag level, but the standard's headline features (reflection, contracts, a finished `std::execution`) are still prerelease at the time of this writing, so this book does not teach against a moving target. Appendix A, the build and toolchain reference, is the designated hook for revisiting C++26 once the ecosystem settles; nothing in this book quietly assumes it along the way.

## Verifying your setup

`scripts/check-prereqs.sh` checks the floor versions for clang, CMake, Ninja, and Conan and prints exactly what it found, without touching Conan or building anything:

```console
$ ./scripts/check-prereqs.sh
Checking local toolchain for Design Patterns with C++...

OK: clang++ -> clang version 22.1.8 (Fedora 22.1.8-4.fc44)
OK: cmake -> cmake version 4.3.0
OK: ninja -> 1.13.2
OK: conan -> Conan version 2.33.0

All prerequisites found.
```

Your own output will show whatever versions `dnf` and `pip` installed for you, which is expected; the script exits non-zero and names the specific missing or too-old tool if something falls short of the floor, rather than letting a mysterious CMake or Conan failure be the first signal something is wrong.

## Using this book in CLion

Open the repository as a project folder. CLion detects `CMakePresets.json` automatically and offers `cpp23` and `cpp20` as configurations in the target selector next to the run and debug buttons, the same two presets `scripts/build.sh` drives from the command line. Selecting `cpp23 | GoogleTest` style targets from that dropdown configures, builds, and runs an individual test binary with full debugger support, breakpoints included, against the exact Conan-generated toolchain file the command line uses. There is nothing CLion-specific to configure beyond letting the IDE's own CMake integration pick up the presets that already exist in the repository.

## Using this book in VS Code

With the CMake Tools extension installed, run **CMake: Select Configure Preset** from the command palette and choose `cpp23` or `cpp20`. CMake Tools reads the same `CMakePresets.json`, configures into the same `build/<preset>/` directory `scripts/build.sh` would use, and exposes a status-bar build button and a Test Explorer view that lists every `gtest_discover_tests` case CTest found. Because the binary directory and toolchain file are identical whether you build from VS Code, from CLion, or from the terminal, you can switch between them freely mid-chapter without reconfiguring from scratch.

## The codetabs convention

Nearly every code sample from here forward appears twice: once under a **C++20** tab and once under a **C++23** tab, switchable with a click, synced across every tab group on the page, and remembered the next time you load a chapter. The point is not novelty. A pattern's shape rarely changes between the two standards, but its idiomatic expression often does: a C++20 sample might build a result type by hand with an `enum` and a union, where the C++23 tab reaches for `std::expected`; a C++20 sample might assemble output with `std::format` into a stream, where C++23 reaches for `std::println` directly. Seeing both side by side, for the same example, is what lets you recognize which standard a piece of code you encounter elsewhere is written against, and what modernizing it would actually involve.

Here is the convention itself, applied to the smallest possible example: printing a line once C++20's way and once C++23's way.

{% include codetabs.html langs="C++20|C++23" %}

```cpp
// C++20: std::format builds the string, the stream writes it.
#include <format>
#include <iostream>

void report_startup(std::string_view component, int port) {
    std::cout << std::format("{} listening on port {}\n", component, port);
}
```
```cpp
// C++23: std::println formats and writes in one call, no stream needed.
#include <print>

void report_startup(std::string_view component, int port) {
    std::println("{} listening on port {}", component, port);
}
```

Both compile under this chapter's toolchain, both produce the same line of output, and both appear in every chapter from here on exactly this way: one marker, then two fenced code blocks, C++20 first. C++26 is never a third tab; when it stops being prerelease, Appendix A is where that decision gets revisited, not a silent addition to every chapter's tab bar.

## How this book works

Every chapter after this one follows the same shape: learning objectives up front, a diagram, a "why this matters" section before any mechanics, the body, and a "for deeper coverage" pointer at the end to the books behind this book's opinions. Chapters group into parts, numbered Part 0 through Part 11, each with its own landing page listing the chapters inside it; this chapter is the first in Part 0, Orientation. Every pattern chapter additionally closes with a "when not to use it" section, because a pattern applied past the point it still pays for itself is itself a mistake, named as clearly as the pattern itself, and carries exactly one short sidebar on how an AI pairing partner tends to reach for that pattern, and where its judgment needs a check.

Every pattern chapter also ships a runnable example under `examples/NN-pattern-name/`, built from new, real-domain C++ written for this book: a reservation system, a telemetry pipeline, a billing engine, never a placeholder `Foo`/`Bar` class hierarchy. Each example carries a GoogleTest suite that verifies the pattern's claimed behavior, not just that the binary compiles and exits zero, discovered automatically by `ctest` through `gtest_discover_tests`. Once the build loop in this chapter works for you, it works identically for every one of those examples: `scripts/build.sh`, then `scripts/verify.sh`, same two presets, same output shape, from the first chapter to the last.

## For deeper coverage

- *GoogleTest in Practice* (Hyuk Myeong, Manning) and the official GoogleTest documentation: the testing idiom every example's GoogleTest suite in this book follows, starting with the first hands-on chapter.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.

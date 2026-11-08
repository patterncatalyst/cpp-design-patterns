---
title: "Prerequisites and the toolchain"
order: 0
part: "orientation"
description: "The clang, gcc, CMake, Ninja, and Conan toolchain on Fedora 44, the cpp23/cpp20 presets, the codetabs convention, and how every example is built, run, and tested."
duration: "20 min"
---

## Learning objectives

By the end of this chapter you can:

- Install and verify the local toolchain every example builds with: clang, CMake, Ninja, and Conan 2.x, on Fedora 44.
- Run the one-time `conan profile detect` step and say what it records.
- Build and test any example with `scripts/build.sh` and `scripts/verify.sh`, under both the `cpp23` and `cpp20` presets.
- Drive the same presets from CLion or VS Code.
- Read a codetabs block, and say why C++26 isn't a third tab.

## Why this matters

Every example compiles, carries a GoogleTest suite, and builds on the host with no container in between. The loop is the same for all of them: `conan install`, `cmake --preset`, `cmake --build`, `ctest`. Get it working once and it works for every chapter, from a terminal or an IDE.

{% include excalidraw.html name="00-build-and-test-loop" caption="Figure 0.1: The local build-and-test loop every example in this book runs: edit source, conan install, cmake --preset (configure), cmake --build (clang), ctest (GoogleTest), observe behavior, and back to the source on a failing test. Both presets, cpp23 and cpp20, run this identical loop." %}

## Local tools, no containers

The reference platform is Fedora 44 with the toolchain installed on the host. Patterns live in source files, translation units and link steps, and you see more of that when you watch `CMakePresets.json` pick a generator and Conan resolve a dependency than when a container does it for you. CI builds in a `fedora:44` container only to get a clean userspace on every run; you never need one locally.

## The stack

| Tool | Version | Job |
| --- | --- | --- |
| clang | 21 or newer (verified with 22) | Default compiler. Every preset pins `CMAKE_CXX_COMPILER` to `clang++`. |
| GCC | optional | Alternate compiler. The examples are verified against clang. |
| CMake | 3.28 or newer (verified with 4.3) | Describes each example's targets and tests. Every `CMakeLists.txt` declares `cmake_minimum_required(VERSION 3.28)`. |
| Ninja | any current release | The generator every preset names. Fast incremental rebuilds. |
| Conan | 2.x | Resolves the one third-party dependency, GoogleTest 1.17.0, and writes the CMake toolchain file the presets load. |

To build with GCC, override the compiler when you configure: `cmake --preset cpp23 -DCMAKE_CXX_COMPILER=g++`. Nothing else changes.

Conan pins GoogleTest in `conanfile.py`, so every reader gets the same version without building it from source and wiring it into each `CMakeLists.txt` by hand.

## Installing on Fedora 44

Four of the five tools come from Fedora's repositories:

```bash
sudo dnf install -y clang lld gcc-c++ cmake ninja-build python3-pip
```

`lld` is clang's linker; the presets don't force it, but it's worth having. `gcc-c++` is the alternate compiler.

Fedora also packages Conan. Installing it with `pip` tracks upstream releases instead of the distribution's rebuild cycle, and any 2.x release runs the same `conanfile.py`:

```bash
pip install --user --upgrade conan
```

Make sure `~/.local/bin` is on your `PATH`, then check:

```bash
conan --version
# Conan version 2.30.0
```

## The one-time step: `conan profile detect`

Conan needs a profile describing the machine: operating system, compiler, compiler version, architecture and default build type. Generate it once:

```bash
conan profile detect
```

Conan inspects the installed compiler and writes `~/.conan2/profiles/default`. Every `conan install` reads it, so you never pass compiler flags to Conan by hand. After a compiler upgrade or a Fedora upgrade, rerun it with `--force` instead of editing the file.

## The build loop

Two scripts wrap the four steps:

```bash
./scripts/build.sh cpp23    # conan install -> cmake --preset cpp23 -> cmake --build
./scripts/verify.sh cpp23   # ctest --preset cpp23 --output-on-failure
```

| Step | What runs |
| --- | --- |
| Install | `conan install . --output-folder=build/cpp23 --build=missing -s compiler.cppstd=23 -s build_type=Release` resolves GoogleTest and writes a toolchain file into `build/cpp23/generators/`. |
| Configure | `cmake --preset cpp23` loads that toolchain file and generates Ninja files. |
| Build | `cmake --build --preset cpp23` compiles every example the root `CMakeLists.txt` finds under `examples/`. |
| Test | `ctest --preset cpp23` runs every `TEST` and `TEST_F` case that `gtest_discover_tests` registered, and reports pass or fail per case, not per binary. |

Every example also builds standalone from its own `examples/NN-name/` directory with the same four commands, and each has a `demo.sh` that does so and runs its demo.

## The two presets

`CMakePresets.json` defines two configure presets, one per standard:

| Preset | Standard | Role |
| --- | --- | --- |
| `cpp23` | C++23 | Default. The primary code targets this standard. |
| `cpp20` | C++20 | Compatibility. The same example, built and tested against the older standard a codebase may still require. |

Each builds into `build/<preset>/`, so both can exist side by side, and both scripts take the preset as their first argument.

There's no `cpp26` preset. Clang accepts `-std=c++2c`, but C++26's headline features (reflection, contracts, a finished `std::execution`) are still prerelease. Appendix A, the build and toolchain reference, is where C++26 gets revisited.

## Verifying your setup

`scripts/check-prereqs.sh` checks the floor versions for clang, CMake, Ninja and Conan and prints what it found, without building anything:

```console
$ ./scripts/check-prereqs.sh
Checking local toolchain for Design Patterns with C++...

OK: clang++ -> clang version 22.1.8 (Fedora 22.1.8-4.fc44)
OK: cmake -> cmake version 4.3.0
OK: ninja -> 1.13.0.git.kitware.jobserver-pipe-1
OK: conan -> Conan version 2.30.0

All prerequisites found.
```

Your versions will differ. If a tool is missing or below the floor, the script names it and exits non-zero, so the first failure you see isn't a confusing CMake or Conan error.

## CLion and VS Code

| IDE | Setup |
| --- | --- |
| CLion | Open the repository folder. CLion reads `CMakePresets.json` and offers `cpp23` and `cpp20` in the configuration selector. Each test binary runs and debugs against the same Conan toolchain file the command line uses. |
| VS Code | Install the CMake Tools extension and run **CMake: Select Configure Preset**. It builds into the same `build/<preset>/` directory and lists every discovered GoogleTest case in its Test Explorer view. |

The binary directory and toolchain file are the same everywhere, so you can switch between the terminal and either IDE without reconfiguring.

## The codetabs convention

Most code samples appear twice, under a **C++20** tab and a **C++23** tab. The tabs sync across the page and the choice is remembered. A pattern's shape rarely changes between the two standards, but its idiomatic expression often does: C++20 builds a result type by hand where C++23 uses `std::expected`, and C++20 formats into a stream where C++23 calls `std::println`.

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

Both produce the same line. The markup is always one marker followed by two fenced blocks, C++20 first. C++26 is never a third tab.

## How the chapters are laid out

Each chapter opens with learning objectives, a diagram and "Why this matters", then the body, and ends with "For deeper coverage". Parts run from Part 0 to Part 11; this is the first chapter of Part 0, Orientation.

Pattern chapters add three things:

- a runnable example under `examples/NN-pattern-name/`, in a real domain such as shipping, billing or telemetry, with a GoogleTest suite that checks the pattern's behavior;
- a "When not to use it" section;
- one AI sidebar on how an AI pairing partner tends to misuse the pattern.

## For deeper coverage

- *GoogleTest in Practice* (Hyuk Myeong, self-published) and the official [GoogleTest documentation](https://google.github.io/googletest/): the testing idiom every example's GoogleTest suite follows.
- The full reading list behind this book's opinions lives on the [bibliography page]({{ '/bibliography/' | relative_url }}); each citation there is a pointer to the full book, not a substitute for reading it.

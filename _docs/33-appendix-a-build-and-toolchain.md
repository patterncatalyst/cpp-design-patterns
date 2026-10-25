---
title: "Build and toolchain reference"
order: 33
layout: tutorial
sectionid: docs
part: appendices
appendix: "A"
description: "A one-stop reference for building and testing every example in this book: the pinned toolchain, the two CMake presets, how Conan supplies GoogleTest, the per-example and whole-repo build paths, and the hook for revisiting C++26 once its headline features settle."
duration: "15 min"
---

## What this appendix covers

Every example in this book builds the same way, with the same local tools and no
containers. This appendix gathers the mechanics in one place: the toolchain to
install, the two build presets, how Conan supplies GoogleTest, and the two ways to
build (one example on its own, or the whole repository at once). It closes with
the hook for adding C++26 later.

The prerequisites chapter walks a first-time setup end to end. This appendix is
the reference you return to once the toolchain is installed.

## Reference platform

The reference platform is Fedora. Everything runs with the host's own compiler,
build tools, and package manager. There is no container anywhere in the build or
test loop, and no example ever asks a reader to run one. Any recent Linux
distribution with a C++20-capable toolchain works; the pinned versions below are
what the examples are verified against.

## The pinned toolchain

`scripts/check-prereqs.sh` verifies the floor version of each tool and prints what
it found. The floors, and the versions the examples are verified against:

| Tool | Floor | Verified against | Role |
|------|-------|------------------|------|
| Clang | 21 | 22.1.8 | Default compiler; C++23 support including `std::print` and `std::expected` |
| GCC | - | 16.2.1 | Supported alternate compiler |
| CMake | 3.28 | 4.3.0 | Build system; the `3.28` floor is what `cmake_minimum_required` sets |
| Ninja | present | 1.13.2 | Default generator |
| Conan | 2.x | 2.x | Supplies third-party dependencies |
| GoogleTest | 1.17.0 | 1.17.0 | Test framework, including GoogleMock; installed via Conan |

Run the check from the repository root:

```sh
./scripts/check-prereqs.sh
```

It reports `OK`, `MISSING`, or `WARNING` per tool and never installs anything, so
it is safe to run at any time.

## The two build presets

The book shows every standard-dependent example side by side in C++20 and C++23,
so the build offers one preset per standard. Both live in each example's
`CMakePresets.json` and in the repository root's.

| Preset | Standard | Role |
|--------|----------|------|
| `cpp23` | C++23 | The default. Code in the book's codetabs leads with this standard. |
| `cpp20` | C++20 | The compatibility build. C++23-only code is guarded so this path always compiles. |

Both presets set the same cache variables: the Clang compiler, the C++ standard,
`CMAKE_CXX_STANDARD_REQUIRED=ON`, `CMAKE_BUILD_TYPE=Release`, and
`CMAKE_EXPORT_COMPILE_COMMANDS=ON`. The last one writes a `compile_commands.json`
that editors read, which is covered below.

There is no `cpp26` preset. The reasoning, and where a future one would go, is in
the C++26 section at the end.

## How Conan supplies GoogleTest

Each example has its own `conanfile.py` that requires `gtest/1.17.0` and generates
the CMake toolchain and dependency files Conan produces. The recipe mirrors the
repository root's, so an example builds on its own without configuring anything
else. When the whole repository builds at once, the root `conanfile.py` supplies
GoogleTest once and the per-example recipes are not invoked.

A Conan install writes its generated files into the preset's build directory:

```sh
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
```

The `--build=missing` flag builds any dependency that has no prebuilt binary for
the current configuration rather than failing.

## Building one example on its own

Every example under `examples/` is standalone-buildable. A reader who cares about
one pattern never has to configure the whole repository. From an example
directory:

```sh
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23
cmake --build --preset cpp23
ctest --preset cpp23 --output-on-failure
```

Swap `cpp23` for `cpp20` and `compiler.cppstd=23` for `20` to build the
compatibility path. Each example also ships a `demo.sh` that runs this sequence
and then the example's demo binary:

```sh
./demo.sh cpp23
```

## Building the whole repository

The repository root carries a superbuild that configures and builds every example
in one pass, for local work and for continuous integration. It discovers each
example by globbing `examples/*/CMakeLists.txt` and adds each one as a
subdirectory, so a new example is picked up with no change to the root build file.

Two scripts wrap the sequence:

```sh
./scripts/build.sh cpp23      # conan install + configure + build, whole repo
./scripts/verify.sh cpp23     # run every example's tests through CTest
```

Each example's `CMakeLists.txt` guards its `project()` and `enable_testing()`
calls behind a check for whether it is the top-level project, so the same file
works standalone and inside the superbuild.

## Tests run through CTest

Every example registers its GoogleTest cases with `gtest_discover_tests`, which
asks the compiled test binary for its test list at build time and registers each
case with CTest individually. `ctest` then runs them and reports per case. A test
asserts behavior, that a pattern does what it claims, not merely that a binary
exits zero.

```sh
ctest --preset cpp23 --output-on-failure
```

GoogleMock ships with GoogleTest through the same Conan package, so examples that
substitute a collaborator with a test double need no extra dependency.

## compile_commands.json and editor diagnostics

Both presets export a `compile_commands.json` into the build directory. Point an
editor's language server (clangd, or the CLion and VS Code C++ tooling) at it so
the editor parses each file with the same standard and include paths the build
uses.

Without that file, a language server guesses, and the guess is usually an older
standard with no package include paths. That produces diagnostics that are not
real: a C++23 type reported as unknown, a GoogleTest header reported as missing, a
standard-library name reported as absent. The build is the source of truth. If an
example builds and its tests pass under both presets, an editor squiggle that the
build does not reproduce is a parsing artifact, not a defect.

## The C++26 revisit hook

This book covers C++20 and C++23. It does not cover C++26. The toolchain accepts
`-std=c++2c` at the flag level, but the headline C++26 features are still
prerelease across compilers and standard libraries, so building the book's
examples against them would teach habits that may not survive standardization.

When C++26 support settles enough to add, the change is small and local:

- Add a `cpp26` configure, build, and test preset to `CMakePresets.json`, both at
  the repository root and in each example, setting `CMAKE_CXX_STANDARD` to `26`.
- Extend the codetabs include to a third label, so a chapter can show C++20, C++23,
  and C++26 side by side where a feature differs.
- Guard any C++26-only code with `#if __cplusplus >= 202602L`, the same way
  C++23-only code is guarded today with `#if __cplusplus >= 202302L`, so the
  C++20 and C++23 build paths keep compiling.

Nothing else in the build depends on the standard version, so adding C++26 does
not touch the superbuild, the Conan recipes, or the test registration.

## For deeper coverage

- Hyuk Myeong, *Googletest in Practice*, self-published: the GoogleTest and
  GoogleMock idioms every example in this book is verified against, read alongside
  the official [GoogleTest documentation](https://google.github.io/googletest/).
- The official [CMake documentation](https://cmake.org/documentation/) for the
  preset and `gtest_discover_tests` mechanics this build relies on, and the
  official [Conan documentation](https://docs.conan.io/2/) for the dependency and
  toolchain generators.

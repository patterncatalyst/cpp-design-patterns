---
title: "Build and toolchain reference"
order: 33
layout: tutorial
sectionid: docs
part: appendices
appendix: "A"
description: "A one-stop reference for building and testing every example: the pinned toolchain, the two CMake presets, how Conan supplies GoogleTest, the per-example and whole-repo build paths, and the hook for revisiting C++26 once its headline features settle."
duration: "15 min"
---

## Learning objectives

By the end of this appendix you can:

- Name the pinned toolchain and the floor for each tool.
- Build one example on its own or the whole repository, under either preset.
- Point an editor at `compile_commands.json` so its diagnostics match the build.
- Add a `cpp26` preset when C++26 support settles.

## Why this matters

Every example builds the same way, with the host's own compiler, build tools and package manager, and no containers. First-time setup is in [Prerequisites and the toolchain]({{ '/docs/00-prerequisites/' | relative_url }}); this page is the reference once the toolchain is installed.

The reference platform is Fedora 44. Any recent Linux distribution with a C++20-capable toolchain works.

## The pinned toolchain

| Tool | Floor | Verified against | Role |
|------|-------|------------------|------|
| Clang | 21 | 22.1.8 | Default compiler; C++23 support including `std::print` and `std::expected` |
| GCC | - | 16.2.1 | Supported alternate compiler |
| CMake | 3.28 | 4.3.0 | Build system; `cmake_minimum_required(VERSION 3.28)` in every `CMakeLists.txt` |
| Ninja | present | 1.13 | Generator named by every preset |
| Conan | 2.x | 2.30.0 | Supplies third-party dependencies |
| GoogleTest | 1.17.0 | 1.17.0 | Test framework, including GoogleMock; installed via Conan |

`scripts/check-prereqs.sh` checks Clang, CMake, Ninja and Conan and prints the version it finds. GCC is an alternate, so the script doesn't check it.

```sh
./scripts/check-prereqs.sh
```

It reports `OK`, `MISSING` or `WARNING` per tool and installs nothing, so it's safe to run at any time.

## The two build presets

Standard-dependent code appears side by side in C++20 and C++23, so the build has one preset per standard. Both are in each example's `CMakePresets.json` and in the repository root's.

| Preset | Standard | Role |
|--------|----------|------|
| `cpp23` | C++23 | The default. Codetabs lead with this standard. |
| `cpp20` | C++20 | The compatibility build. C++23-only code is guarded so this path always compiles. |

Both set the same cache variables: Clang as the compiler, the C++ standard, `CMAKE_CXX_STANDARD_REQUIRED=ON`, `CMAKE_BUILD_TYPE=Release` and `CMAKE_EXPORT_COMPILE_COMMANDS=ON`. There's no `cpp26` preset; see the last section.

## How Conan supplies GoogleTest

Each example has its own `conanfile.py` that requires `gtest/1.17.0` and generates the CMake toolchain and dependency files. It mirrors the root recipe, so an example builds on its own. In the whole-repository build the root `conanfile.py` supplies GoogleTest once and the per-example recipes aren't invoked.

Conan writes its generated files into the preset's build directory:

```sh
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
```

`--build=missing` builds any dependency that has no prebuilt binary for the current configuration instead of failing.

GoogleMock ships in the same Conan package, so an example that substitutes a collaborator with a test double needs no extra dependency.

## Building one example on its own

Every example under `examples/` builds standalone. From an example directory:

```sh
conan install . --output-folder=build/cpp23 --build=missing \
    -s compiler.cppstd=23 -s build_type=Release
cmake --preset cpp23
cmake --build --preset cpp23
ctest --preset cpp23 --output-on-failure
```

For the compatibility path, swap `cpp23` for `cpp20` and `compiler.cppstd=23` for `20`. Each example also ships a `demo.sh` that runs this sequence and then the demo binary:

```sh
./demo.sh cpp23
```

## Building the whole repository

The root `CMakeLists.txt` is a superbuild. It globs `examples/*/CMakeLists.txt` and adds each match with `add_subdirectory`, so a new example needs no change to the root build file. Two scripts wrap the sequence:

```sh
./scripts/build.sh cpp23      # conan install + configure + build, whole repo
./scripts/verify.sh cpp23     # run every example's tests through CTest
```

Each example's `CMakeLists.txt` calls `project()` and `enable_testing()` only when it's the top-level project, so the same file works standalone and inside the superbuild.

## Tests run through CTest

Every example registers its GoogleTest cases with `gtest_discover_tests`, which asks the compiled test binary for its test list at build time and registers each case with CTest individually. `ctest` runs them and reports per case.

```sh
ctest --preset cpp23 --output-on-failure
```

## compile_commands.json and editor diagnostics

Both presets write `compile_commands.json` into the build directory. Point the editor's language server (clangd, or the CLion and VS Code C++ tooling) at it, and the editor parses each file with the build's standard and include paths.

Without that file a language server guesses, usually an older standard with no package include paths. The result is diagnostics the build doesn't reproduce: a C++23 type reported as unknown, a GoogleTest header reported as missing. The build is the source of truth. If an example builds and passes under both presets, a squiggle the build doesn't reproduce is a parsing artifact.

## The C++26 revisit hook

Coverage stops at C++23. Clang accepts `-std=c++2c`, but the headline C++26 features are still prerelease across compilers and standard libraries. Adding C++26 is a local change:

- Add a `cpp26` configure, build and test preset to `CMakePresets.json`, at the root and in each example, with `CMAKE_CXX_STANDARD` set to `26`.
- Extend the codetabs include to a third label so a section can show C++20, C++23 and C++26 side by side.
- Guard C++26-only code with `#if __cplusplus >= 202602L`, as C++23-only code is guarded today with `#if __cplusplus >= 202302L`, so the older presets keep compiling.

Nothing else depends on the standard version, so the superbuild, the Conan recipes and the `gtest_discover_tests` registration don't change.

## For deeper coverage

- Hyuk Myeong, *Googletest in Practice*, self-published: the GoogleTest and
  GoogleMock idioms every example follows, read alongside
  the official [GoogleTest documentation](https://google.github.io/googletest/).
- The official [CMake documentation](https://cmake.org/documentation/) for the
  preset and `gtest_discover_tests` mechanics this build relies on, and the
  official [Conan documentation](https://docs.conan.io/2/) for the dependency and
  toolchain generators.

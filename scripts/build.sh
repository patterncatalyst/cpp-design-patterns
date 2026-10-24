#!/bin/sh
# Usage: scripts/build.sh [cpp23|cpp20]
#
# Install this repo's one Conan dependency (GoogleTest, pinned in
# conanfile.py), configure with the matching CMakePresets.json preset, and
# build. No containers — local clang + CMake + Ninja + Conan only. Run
# scripts/check-prereqs.sh first if you haven't verified the toolchain.
#
# Defaults to the cpp23 preset, the book's default standard. Pass cpp20 to
# build under the compatibility standard instead. There is no cpp26 preset;
# C++26 support is deferred (see Appendix A).

set -eu

preset="${1:-cpp23}"

case "${preset}" in
    cpp23) cppstd=23 ;;
    cpp20) cppstd=20 ;;
    *)
        echo "Usage: $0 [cpp23|cpp20]" >&2
        exit 1
        ;;
esac

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "${repo_root}"

echo "==> conan install (preset: ${preset}, compiler.cppstd=${cppstd})"
conan install . \
    --output-folder="build/${preset}" \
    --build=missing \
    -s "compiler.cppstd=${cppstd}" \
    -s build_type=Release

echo "==> cmake --preset ${preset}"
cmake --preset "${preset}"

echo "==> cmake --build --preset ${preset}"
cmake --build --preset "${preset}"

echo "Build complete. Run scripts/verify.sh ${preset} to run the test suite."

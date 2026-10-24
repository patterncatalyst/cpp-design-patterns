#!/bin/sh
# Usage: ./demo.sh [cpp23|cpp20]
#
# Builds examples/06-value-semantics-ownership standalone (conan install +
# cmake preset + build) and runs the ownership-model demo. Run from this
# directory. No containers — local clang + CMake + Ninja + Conan only.

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

script_dir="$(cd "$(dirname "$0")" && pwd)"
cd "${script_dir}"

echo "==> conan install (preset: ${preset}, compiler.cppstd=${cppstd})"
conan install . \
    --output-folder="build/${preset}" \
    --build=missing \
    -s "compiler.cppstd=${cppstd}" \
    -s build_type=Release

echo "==> cmake --preset ${preset}"
cmake --preset "${preset}"

echo "==> cmake --build --preset ${preset} --target value6_ownership_demo"
cmake --build --preset "${preset}" --target value6_ownership_demo

echo
echo "==> running the demo (build/${preset}/value6_ownership_demo)"
"./build/${preset}/value6_ownership_demo"

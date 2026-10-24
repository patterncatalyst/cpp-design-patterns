#!/bin/sh
# Usage: scripts/verify.sh [cpp23|cpp20]
#
# Run CTest (which discovers every example's GoogleTest cases via
# gtest_discover_tests) against an already-built preset. Run
# scripts/build.sh <preset> first — this script does not build.

set -eu

preset="${1:-cpp23}"

case "${preset}" in
    cpp23|cpp20) ;;
    *)
        echo "Usage: $0 [cpp23|cpp20]" >&2
        exit 1
        ;;
esac

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "${repo_root}"

echo "==> ctest --preset ${preset}"
ctest --preset "${preset}" --output-on-failure

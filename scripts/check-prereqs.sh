#!/bin/sh
# Usage: scripts/check-prereqs.sh
#
# Verify the local toolchain this book's examples build with, and print the
# version found for each tool. No containers: everything here runs on the
# host (Fedora 44 is the reference platform; see _docs/00-prerequisites.md).
#
# Pinned/floor versions, from the r01 build plan's verified toolchain table:
#   clang   >= 21   (verified against 22.1.8)
#   cmake   >= 3.28 (verified against 4.3.0)
#   ninja   present (verified against 1.13.2)
#   conan   2.x     (verified against 2.25.2)
#   gtest   1.17.0, installed via Conan — not checked here, see conanfile.py

set -eu

status=0

have() {
    command -v "$1" >/dev/null 2>&1
}

check_clang() {
    if ! have clang++; then
        echo "MISSING: clang++ (need clang >= 21)"
        status=1
        return
    fi
    version="$(clang++ --version | head -n1)"
    major="$(clang++ --version | head -n1 | sed -n 's/.*version \([0-9]*\)\..*/\1/p')"
    echo "OK: clang++ -> ${version}"
    if [ -n "${major}" ] && [ "${major}" -lt 21 ]; then
        echo "WARNING: clang major version ${major} is below the floor of 21"
        status=1
    fi
}

check_cmake() {
    if ! have cmake; then
        echo "MISSING: cmake (need >= 3.28)"
        status=1
        return
    fi
    version="$(cmake --version | head -n1)"
    echo "OK: cmake -> ${version}"
}

check_ninja() {
    if ! have ninja; then
        echo "MISSING: ninja"
        status=1
        return
    fi
    echo "OK: ninja -> $(ninja --version)"
}

check_conan() {
    if ! have conan; then
        echo "MISSING: conan (need 2.x)"
        status=1
        return
    fi
    version="$(conan --version)"
    echo "OK: conan -> ${version}"
    case "${version}" in
        *"Conan version 1."*)
            echo "WARNING: Conan 1.x detected; this project requires Conan 2.x"
            status=1
            ;;
    esac
}

echo "Checking local toolchain for Design Patterns with C++..."
echo
check_clang
check_cmake
check_ninja
check_conan
echo

if [ "${status}" -eq 0 ]; then
    echo "All prerequisites found."
else
    echo "One or more prerequisites are missing or below the floor version. See _docs/00-prerequisites.md."
fi

exit "${status}"

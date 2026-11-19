#!/usr/bin/env bash
# run.sh — one-stop build/test/run helper for MiniQuantExchange.
#
# You don't need to remember any cmake flags — just run this file.
#
#   ./run.sh            configure + build everything, run tests, run demo
#   ./run.sh build       configure + build only
#   ./run.sh test        build (if needed) + run the test suite
#   ./run.sh demo        build (if needed) + run the miniq_demo CLI
#   ./run.sh playground   build (if needed) + run all playground executables
#   ./run.sh asan        configure + build with ASan/UBSan + run tests under it
#   ./run.sh clean       remove build directories
#   ./run.sh all         build + test + demo + playground (same as no args)
#
# Requires: CMake 3.20+, a C++20 compiler. First run needs network access
# once (Catch2 is fetched automatically by CMake).

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
ASAN_BUILD_DIR="${PROJECT_DIR}/build-asan"
JOBS="$(command -v nproc >/dev/null 2>&1 && nproc || echo 4)"

configure_and_build() {
    echo "==> Configuring (${BUILD_DIR})"
    cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug
    echo "==> Building (-j${JOBS})"
    cmake --build "${BUILD_DIR}" -j"${JOBS}"
}

run_tests() {
    configure_and_build
    echo "==> Running tests"
    "${BUILD_DIR}/miniq_tests"
}

run_demo() {
    configure_and_build
    echo "==> Running demo"
    "${BUILD_DIR}/miniq_demo"
}

run_playground() {
    configure_and_build
    for exe in cpp_fundamentals copy_move_raii polymorphism templates_stl concurrency; do
        echo "==> playground_${exe}"
        "${BUILD_DIR}/playground_${exe}"
        echo
    done
}

run_asan() {
    echo "==> Configuring with sanitizers (${ASAN_BUILD_DIR})"
    cmake -S "${PROJECT_DIR}" -B "${ASAN_BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug -DMINIQ_ENABLE_SANITIZERS=ON
    echo "==> Building (-j${JOBS})"
    cmake --build "${ASAN_BUILD_DIR}" -j"${JOBS}"
    echo "==> Running tests under ASan/UBSan"
    "${ASAN_BUILD_DIR}/miniq_tests"
}

run_clean() {
    echo "==> Removing ${BUILD_DIR} and ${ASAN_BUILD_DIR}"
    rm -rf "${BUILD_DIR}" "${ASAN_BUILD_DIR}"
}

run_all() {
    run_tests
    echo
    run_demo
    echo
    run_playground
}

case "${1:-all}" in
    build)      configure_and_build ;;
    test)       run_tests ;;
    demo)       run_demo ;;
    playground) run_playground ;;
    asan)       run_asan ;;
    clean)      run_clean ;;
    all)        run_all ;;
    *)
        echo "Unknown command: ${1}"
        echo "Usage: ./run.sh [build|test|demo|playground|asan|clean|all]"
        exit 1
        ;;
esac

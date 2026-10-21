#!/usr/bin/env bash
# Build SudokuCode with clang on Linux or Windows (MSYS2/WSL/MinGW).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

BUILD_GUI=OFF
BUILD_TESTS=OFF
CLEAN=0

for arg in "$@"; do
    case "$arg" in
        clean) CLEAN=1 ;;
        gui) BUILD_GUI=ON ;;
        test) BUILD_TESTS=ON ;;
        all) BUILD_GUI=ON; BUILD_TESTS=ON ;;
    esac
done

if [[ "$CLEAN" == "1" ]]; then
    rm -rf "$BUILD_DIR"
fi

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_COMPILER=clang \
    -DBUILD_GUI="$BUILD_GUI" \
    -DBUILD_TESTS="$BUILD_TESTS" \
    "${CMAKE_EXTRA_ARGS:-}"

cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"

if [[ "$BUILD_TESTS" == "ON" ]]; then
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

echo "Build complete."
if [[ "$BUILD_GUI" == "ON" ]]; then
    echo "  GUI:     $BUILD_DIR/sudokucode_gui"
fi

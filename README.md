# SudokuCode

A scannable 2D barcode format where a Sudoku grid stores data.
Instead of digits 1-9, each cell holds a hexadecimal value from `00` to `FF`.

## Format

A SudokuCode is stored as a BMP image. Each cell of the grid holds one
hexadecimal byte value (`00`-`FF`) encoded as a grayscale cell in the image.

- Grid dimensions: `4x4`, `9x9`, or `16x16` cells.
- Each cell holds one byte `00`-`FF`.
- Every row, column, and box contains unique non-zero values.
- The first `length` cells (row-major) are the encoded data (payload + CRC32 +
  optional Reed-Solomon parity); remaining cells are filled by a Sudoku solver.
- `00` marks an empty (solved) slot; the mask tracks which cells hold data.

## Features

- **Reed-Solomon error correction** (levels 0-3) to reconstruct damaged cells.
- **CRC32** integrity check on every payload.
- **Optional XOR encryption** (`--encrypt --key`).
- **BMP image export/import** (no external dependencies) for visual scanning.
- **ASCII preview** (`--ascii`) for debugging.
- **Linux only** (POSIX, no Windows GUI).

## Build

```sh
cmake -S . -B build && cmake --build build
```

Run tests:

```sh
cmake -S . -B build -DBUILD_TESTS=ON && cmake --build build && ctest --test-dir build
```

Install:

```sh
cmake --install build
```

## Constraints

- Max payload per grid: `n*n` bytes (16 / 81 / 256 for 4/9/16).
- Data must not create a duplicate in any row/column/box (encoder reports an error).
- Avoid raw `00` bytes unless the mask distinguishes them (it does).
- With ECC level `L`, up to `L/2` cell errors can be corrected.

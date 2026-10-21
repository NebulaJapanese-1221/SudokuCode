# SudokuCode

A scannable 2D barcode format where a Sudoku grid stores data.
Instead of digits 1-9, each cell holds a hexadecimal value from `00` to `FF`.

## Format

A SudokuCode is stored as a BMP image. Each cell of the grid holds one
hexadecimal byte value (`00`-`FF`) encoded as a grayscale cell in the image.
Large payloads are split across multiple Sudoku pages stacked vertically in one
BMP image, each page carrying its own page index, total page count, length, and
CRC32 header.

- Grid dimensions: `4x4`, `9x9`, or `16x16` cells.
- Each cell holds one byte `00`-`FF`.
- Every row, column, and box contains unique non-zero values.
- The first cells of each page (row-major) hold a page header + data; remaining
  cells are filled by a Sudoku solver.
- `00` marks an empty (solved) slot; the mask tracks which cells hold data.

## Features

- **Multi-page chunked encoding** — arbitrarily large payloads across multiple Sudoku grids in one BMP image.
- **Auto ECC selection** — Reed-Solomon redundancy (levels 0-3) chosen automatically based on payload size vs. grid capacity.
- **QR-style finder patterns** — corner alignment markers for camera scanning (`--finder`).
- **Batch mode** — encode/decode multiple files into a single image with an index page.
- **Compression** — zlib compression before ECC (`--compress`).
- **Password-based key derivation** — PBKDF2-HMAC-SHA256 from a passphrase (`--passphrase`).
- **Optional XOR encryption** (`--encrypt --key`).
- **BMP image export/import** (no external dependencies) for visual scanning.
- **Steganographic mode** — embed grids inside ordinary photos via LSB (`--hide`/`--extract`).
- **Verification-only mode** — check integrity without decoding (`--verify`).
- **ASCII preview** (`--ascii`) for debugging.
- **Python bindings** — `sudokucode` Python module (`make py`).
- **Linux only** (POSIX, no Windows GUI).

## Build

```sh
make                # sudokucode_gui (Qt6 GUI)
make py             # sudokucode.so (Python bindings)
```

Requires Qt6, zlib, OpenSSL, and Python 3 dev headers.

Run tests:

```sh
cmake -S . -B build -DBUILD_TESTS=ON && cmake --build build && ctest --test-dir build
```

With Python bindings:

```sh
cmake -S . -B build -DBUILD_TESTS=ON -DBUILD_PYTHON=ON && cmake --build build
```

## Constraints

- Max payload per grid page: `n*n - 10 - nsym` bytes (10-byte page header + Reed-Solomon parity).
- Data must not create a duplicate in any row/column/box (encoder reports an error).
- Avoid raw `00` bytes unless the mask distinguishes them (it does).
- With ECC level `L`, up to `L/2` cell errors per page can be corrected.

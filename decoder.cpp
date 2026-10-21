#include "sudoku.h"
#include <vector>
#include <fstream>
#include <iostream>

using namespace SudokuCode;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: decoder <file.sudokucode> [output_file]\n";
        return 1;
    }
    Grid g; Mask mask; uint8_t length = 0;
    if (!readGridFile(argv[1], g, mask, length)) {
        std::cerr << "Failed to read sudokucode file: " << argv[1] << '\n';
        return 1;
    }
    if (!validateGrid(g)) {
        std::cerr << "Invalid sudoku grid (constraint violation).\n";
        return 1;
    }
    std::vector<uint8_t> data;
    size_t count = 0;
    for (int r = 0; r < GRID && count < length; ++r)
        for (int c = 0; c < GRID && count < length; ++c) {
            data.push_back(g[r][c]);
            ++count;
        }
    if (argc >= 3) {
        std::ofstream ofs(argv[2], std::ios::binary);
        if (!ofs) {
            std::cerr << "Cannot open output: " << argv[2] << '\n';
            return 1;
        }
        ofs.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
        std::cout << "Decoded " << data.size() << " bytes to " << argv[2] << '\n';
    } else {
        for (size_t i = 0; i < data.size(); ++i)
            std::cout << byteToHex(data[i]) << (i + 1 < data.size() ? ' ' : '\n');
        std::cout << "Decoded " << data.size() << " bytes.\n";
    }
    return 0;
}

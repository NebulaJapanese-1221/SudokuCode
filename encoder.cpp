#include "sudoku.h"
#include <vector>
#include <fstream>
#include <iterator>

using namespace SudokuCode;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: encoder <input_file> [output.sudokucode]\n";
        return 1;
    }
    std::ifstream ifs(argv[1], std::ios::binary);
    if (!ifs) {
        std::cerr << "Cannot open input: " << argv[1] << '\n';
        return 1;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
    if (data.size() > 81) {
        std::cerr << "Data too large (max 81 bytes).\n";
        return 1;
    }
    Grid g{}; Mask mask{};
    size_t idx = 0;
    for (int r = 0; r < GRID && idx < data.size(); ++r)
        for (int c = 0; c < GRID && idx < data.size(); ++c) {
            g[r][c] = data[idx++];
            mask[r][c] = true;
        }
    if (!validateGrid(g)) {
        std::cerr << "Data conflicts with sudoku constraints "
                  << "(duplicate value in a row, column, or 3x3 box).\n";
        return 1;
    }
    if (!solve(g, mask)) {
        std::cerr << "Failed to solve sudoku grid.\n";
        return 1;
    }
    std::string out = (argc >= 3) ? argv[2] : "output.sudokucode";
    if (!writeGridFile(out, g, mask, static_cast<uint8_t>(data.size()))) {
        std::cerr << "Failed to write output file.\n";
        return 1;
    }
    std::cout << "Encoded " << data.size() << " bytes to " << out << '\n';
    printGrid(g);
    return 0;
}

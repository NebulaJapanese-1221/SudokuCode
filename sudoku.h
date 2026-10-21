#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace SudokuCode {

static const int GRID = 9;
static const int BOX = 3;
using Grid = std::array<std::array<uint8_t, GRID>, GRID>;
using Mask = std::array<std::array<bool, GRID>, GRID>;

inline bool isValid(const Grid& g, int row, int col, uint8_t val) {
    if (val == 0x00) return true;
    for (int c = 0; c < GRID; ++c)
        if (g[row][c] == val) return false;
    for (int r = 0; r < GRID; ++r)
        if (g[r][col] == val) return false;
    int br = row / BOX, bc = col / BOX;
    for (int r = br * BOX; r < (br + 1) * BOX; ++r)
        for (int c = bc * BOX; c < (bc + 1) * BOX; ++c)
            if (g[r][c] == val) return false;
    return true;
}

inline bool findEmpty(const Grid& g, const Mask& mask, int& row, int& col) {
    for (int r = 0; r < GRID; ++r)
        for (int c = 0; c < GRID; ++c)
            if (!mask[r][c] && g[r][c] == 0x00) { row = r; col = c; return true; }
    return false;
}

inline bool solve(Grid& g, const Mask& mask) {
    int row, col;
    if (!findEmpty(g, mask, row, col)) return true;
    for (int v = 1; v <= 0xFF; ++v) {
        uint8_t val = static_cast<uint8_t>(v);
        if (isValid(g, row, col, val)) {
            g[row][col] = val;
            if (solve(g, mask)) return true;
            g[row][col] = 0x00;
        }
    }
    return false;
}

inline bool validateGrid(const Grid& g) {
    bool seen[256];
    for (int r = 0; r < GRID; ++r) {
        for (int i = 0; i < 256; ++i) seen[i] = false;
        for (int c = 0; c < GRID; ++c) {
            uint8_t v = g[r][c];
            if (v == 0x00) continue;
            if (seen[v]) return false;
            seen[v] = true;
        }
    }
    for (int c = 0; c < GRID; ++c) {
        for (int i = 0; i < 256; ++i) seen[i] = false;
        for (int r = 0; r < GRID; ++r) {
            uint8_t v = g[r][c];
            if (v == 0x00) continue;
            if (seen[v]) return false;
            seen[v] = true;
        }
    }
    for (int br = 0; br < BOX; ++br)
        for (int bc = 0; bc < BOX; ++bc) {
            for (int i = 0; i < 256; ++i) seen[i] = false;
            for (int r = br * BOX; r < (br + 1) * BOX; ++r)
                for (int c = bc * BOX; c < (bc + 1) * BOX; ++c) {
                    uint8_t v = g[r][c];
                    if (v == 0x00) continue;
                    if (seen[v]) return false;
                    seen[v] = true;
                }
        }
    return true;
}

inline void printGrid(const Grid& g) {
    for (int r = 0; r < GRID; ++r) {
        for (int c = 0; c < GRID; ++c) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(g[r][c]);
            if (c < GRID - 1) std::cout << ' ';
        }
        std::cout << '\n';
    }
    std::cout << std::dec;
}

inline std::string byteToHex(uint8_t b) {
    std::ostringstream oss;
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

inline bool parseHexByte(const std::string& tok, uint8_t& out) {
    if (tok.size() != 2) return false;
    unsigned long v;
    try { v = std::stoul(tok, nullptr, 16); }
    catch (...) { return false; }
    if (v > 0xFF) return false;
    out = static_cast<uint8_t>(v);
    return true;
}

inline bool writeGridFile(const std::string& path, const Grid& g,
                          const Mask& mask, uint8_t length) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    ofs << "SC1\n";
    ofs << byteToHex(length) << '\n';
    for (int r = 0; r < GRID; ++r) {
        for (int c = 0; c < GRID; ++c) {
            ofs << byteToHex(g[r][c]);
            if (c < GRID - 1) ofs << ' ';
        }
        ofs << '\n';
    }
    for (int r = 0; r < GRID; ++r) {
        for (int c = 0; c < GRID; ++c) {
            ofs << (mask[r][c] ? "01" : "00");
            if (c < GRID - 1) ofs << ' ';
        }
        ofs << '\n';
    }
    return ofs.good();
}

inline bool readGridFile(const std::string& path, Grid& g, Mask& mask,
                         uint8_t& length) {
    std::ifstream ifs(path);
    if (!ifs) return false;
    std::string magic;
    if (!(ifs >> magic) || magic != "SC1") return false;
    std::string lenTok;
    if (!(ifs >> lenTok)) return false;
    uint8_t lv;
    if (!parseHexByte(lenTok, lv)) return false;
    length = lv;
    for (int r = 0; r < GRID; ++r)
        for (int c = 0; c < GRID; ++c) {
            std::string tok;
            if (!(ifs >> tok)) return false;
            if (!parseHexByte(tok, g[r][c])) return false;
        }
    for (int r = 0; r < GRID; ++r)
        for (int c = 0; c < GRID; ++c) {
            std::string tok;
            if (!(ifs >> tok)) return false;
            mask[r][c] = (tok == "01");
        }
    return true;
}

} // namespace SudokuCode

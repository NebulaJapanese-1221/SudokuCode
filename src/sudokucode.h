#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace sc {

struct Params {
    int n = 9;              // grid dimension: 4, 9, or 16
    int eccLevel = 2;       // Reed-Solomon redundancy level 0..3
    bool encrypt = false;
    std::string key;        // XOR key (repeated)
    bool compress = false;  // reserved for future zlib
};

struct Code {
    int n = 9;
    std::vector<uint8_t> grid;   // n*n solved values
    std::vector<uint8_t> mask;   // n*n: 1 = original data cell, 0 = solved filler
    uint32_t length = 0;         // number of data bytes (incl. CRC)
};

// ---------- encode / decode ----------
std::vector<uint8_t> encode(const std::vector<uint8_t>& data,
                            const Params& p, Code& out);
std::vector<uint8_t> decode(const Code& code, const Params& p);

// ---------- BMP image (no external deps) ----------
bool writeBMP(const std::string& path, const Code& c, int cellSize = 20);
bool readBMP(const std::string& path, Code& c, int cellSize = 20);

// ---------- ASCII preview ----------
std::string asciiPreview(const Code& c);

// ---------- CRC32 ----------
uint32_t crc32(const uint8_t* data, size_t len);

// ---------- Validation ----------
bool validateGrid(const Code& c);

} // namespace sc

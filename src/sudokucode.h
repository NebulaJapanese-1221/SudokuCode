#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace sc {

struct Params {
    int n = 9;              // grid dimension: 4, 9, or 16
    int eccLevel = 2;       // Reed-Solomon redundancy level 0..3 (manual override)
    bool autoEcc = false;   // auto-select ECC based on payload size
    bool encrypt = false;
    std::string key;        // raw XOR key
    std::string passphrase; // passphrase for PBKDF2-HMAC-SHA256 key derivation
    bool compress = false;  // zlib compress before ECC
    bool verifyOnly = false;// verification-only mode (decode, no data output)
};

// One Sudoku page. Multiple pages chained for large payloads.
struct Code {
    int n = 9;
    std::vector<uint8_t> grid;    // n*n solved values
    std::vector<uint8_t> mask;    // n*n: 1 = data cell, 0 = solved filler
    uint32_t length = 0;          // payload data bytes on this page (excl. header)
    uint32_t page = 0;            // 0-based page index
    uint32_t totalPages = 1;      // total number of pages
    uint32_t crc = 0;             // CRC32 of this page's payload data
};

// One entry in a batch encode/decode.
struct BatchEntry {
    std::string filename;
    std::vector<uint8_t> data;
};

// ---------- Key derivation (PBKDF2-HMAC-SHA256) ----------
std::string deriveKey(const std::string& passphrase,
                      const std::string& salt,
                      int iterations);

// ---------- Compression (zlib) ----------
std::vector<uint8_t> compressData(const std::vector<uint8_t>& data);
std::vector<uint8_t> decompressData(const std::vector<uint8_t>& data);

// ---------- Multi-page encode / decode ----------
std::vector<Code> encode(const std::vector<uint8_t>& data, const Params& p);
std::vector<uint8_t> decode(const std::vector<Code>& pages, const Params& p);

// Verification-only: validate all pages + CRCs, return true if intact.
bool verify(const std::vector<Code>& pages, const Params& p);

// ---------- Batch mode (index page + multiple files) ----------
std::vector<Code> encodeBatch(const std::vector<BatchEntry>& files,
                              const Params& p);
std::vector<BatchEntry> decodeBatch(const std::vector<Code>& pages,
                                    const Params& p);

// ---------- BMP image (multi-page, no external deps) ----------
// Pages stacked vertically. Finder patterns in corners when useFinder=true.
bool writeBMP(const std::string& path, const std::vector<Code>& pages,
              int cellSize = 20, bool useFinder = false);
bool readBMP(const std::string& path, std::vector<Code>& pages,
             int cellSize = 20, bool useFinder = false);

// ---------- Steganography (LSB hide/extract in ordinary BMP photo) ----------
bool hideInImage(const std::string& coverPath, const std::string& outPath,
                 const std::vector<Code>& pages, int cellSize = 20);
bool extractFromImage(const std::string& stegoPath,
                      std::vector<Code>& pages, int cellSize = 20);

// ---------- CRC32 ----------
uint32_t crc32(const uint8_t* data, size_t len);

// ---------- Validation ----------
bool validateGrid(const Code& c);

// ---------- ASCII preview (single page) ----------
std::string asciiPreview(const Code& c);

} // namespace sc

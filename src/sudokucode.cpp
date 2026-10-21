#include "sudokucode.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <zlib.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace sc {

// ============================================================================
// CRC32
// ============================================================================
uint32_t crc32(const uint8_t* data, size_t len) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// ============================================================================
// GF(256) for Reed-Solomon, primitive polynomial 0x11D
// ============================================================================
namespace gf {
static uint8_t exp[512];
static uint8_t log[256];

void init() {
    uint8_t x = 1;
    for (int i = 0; i < 255; ++i) {
        exp[i] = x;
        log[x] = static_cast<uint8_t>(i);
        x <<= 1;
        if (x & 0x100) x ^= 0x11D;
    }
    for (int i = 255; i < 512; ++i) exp[i] = exp[i - 255];
    log[0] = 0;
}
uint8_t mul(uint8_t a, uint8_t b) {
    if (a == 0 || b == 0) return 0;
    return exp[log[a] + log[b]];
}
uint8_t div(uint8_t a, uint8_t b) {
    if (b == 0) return 0;
    if (a == 0) return 0;
    return exp[(255 + log[a] - log[b]) % 255];
}
uint8_t inv(uint8_t a) { return exp[255 - log[a]]; }
uint8_t pow(uint8_t a, int e) {
    if (a == 0) return 0;
    return exp[(log[a] * e) % 255];
}
} // namespace gf

// Decode RS codeword r (msg || parity). Returns corrected msg (first k bytes)
// or empty on failure.
static std::vector<uint8_t> rsDecode(const std::vector<uint8_t>& r, int nsym, int k) {
    gf::init();
    if (nsym == 0) {
        return std::vector<uint8_t>(r.begin(), r.begin() + k);
    }
    auto syndPoly = [&](const std::vector<uint8_t>& rr) {
        std::vector<uint8_t> s(nsym, 0);
        for (int i = 0; i < nsym; ++i) {
            uint8_t x = gf::pow(2, i + 1);
            uint8_t acc = 0;
            for (size_t j = 0; j < rr.size(); ++j)
                acc ^= gf::mul(rr[j], gf::pow(x, static_cast<int>(j)));
            s[i] = acc;
        }
        return s;
    };
    auto s = syndPoly(r);
    bool allZero = true;
    for (uint8_t v : s) if (v) { allZero = false; break; }
    if (allZero)
        return std::vector<uint8_t>(r.begin(), r.begin() + k);

    std::vector<uint8_t> lam(1, 1), b(1, 1);
    int l = 0, m = 1;
    uint8_t bb = 1;
    for (int i = 0; i < nsym; ++i) {
        uint8_t delta = s[i];
        for (int j = 1; j <= l; ++j)
            delta ^= gf::mul(lam[j], s[i - j]);
        if (delta == 0) {
            ++m;
        } else if (2 * l <= i) {
            std::vector<uint8_t> t = lam;
            uint8_t coef = gf::mul(delta, gf::inv(bb));
            std::vector<uint8_t> shifted(std::max(b.size(), lam.size()), 0);
            for (size_t j = 0; j < b.size(); ++j)
                shifted[j + m] = gf::mul(b[j], coef);
            for (size_t j = 0; j < shifted.size(); ++j)
                if (j < lam.size()) lam[j] ^= shifted[j];
                else lam.push_back(shifted[j]);
            l = i + 1 - l;
            b = t;
            bb = delta;
            m = 1;
        } else {
            uint8_t coef = gf::mul(delta, gf::inv(bb));
            std::vector<uint8_t> shifted(std::max(b.size(), lam.size()), 0);
            for (size_t j = 0; j < b.size(); ++j)
                shifted[j + m] = gf::mul(b[j], coef);
            for (size_t j = 0; j < shifted.size(); ++j)
                if (j < lam.size()) lam[j] ^= shifted[j];
                else lam.push_back(shifted[j]);
            ++m;
        }
    }
    lam.resize(l + 1);

    int N = static_cast<int>(r.size());
    std::vector<int> errPos;
    for (int i = 0; i < N; ++i) {
        uint8_t x = gf::pow(2, i);
        uint8_t acc = 0;
        uint8_t xp = 1;
        for (size_t j = 0; j < lam.size(); ++j) {
            acc ^= gf::mul(lam[j], xp);
            xp = gf::mul(xp, x);
        }
        if (acc == 0) errPos.push_back(N - 1 - i);
    }
    int numErrors = static_cast<int>(errPos.size());
    if (numErrors == 0 || numErrors > nsym / 2)
        return {};
    if (static_cast<int>(lam.size()) - 1 != numErrors)
        return {};

    std::vector<uint8_t> omega(nsym, 0);
    for (size_t i = 0; i < s.size(); ++i)
        for (size_t j = 0; j < lam.size() && i + j < omega.size(); ++j)
            omega[i + j] ^= gf::mul(s[i], lam[j]);
    std::vector<uint8_t> lamPrime(lam.size(), 0);
    for (size_t j = 1; j < lam.size(); j += 2)
        lamPrime[j] = lam[j];

    std::vector<uint8_t> mag;
    mag.reserve(errPos.size());
    for (int pos : errPos) {
        uint8_t X = gf::pow(2, static_cast<int>(r.size()) - 1 - pos);
        uint8_t num = 0;
        uint8_t xp = 1;
        for (size_t j = 0; j < omega.size(); ++j) {
            num ^= gf::mul(omega[j], xp);
            xp = gf::mul(xp, X);
        }
        uint8_t den = 0;
        xp = 1;
        for (size_t j = 0; j < lamPrime.size(); ++j) {
            den ^= gf::mul(lamPrime[j], xp);
            xp = gf::mul(xp, X);
        }
        if (den == 0) { mag.push_back(0); continue; }
        mag.push_back(gf::mul(num, gf::inv(den)));
    }

    std::vector<uint8_t> corrected = r;
    for (size_t i = 0; i < errPos.size(); ++i)
        corrected[errPos[i]] ^= mag[i];

    auto s2 = syndPoly(corrected);
    for (uint8_t v : s2) if (v) return {};
    return std::vector<uint8_t>(corrected.begin(), corrected.begin() + k);
}

// ============================================================================
// Sudoku solve / validate
// ============================================================================
static int boxSize(int n) { return static_cast<int>(std::sqrt(static_cast<double>(n))); }

static bool sudokuValid(const std::vector<uint8_t>& g, int n, int row, int col, uint8_t val) {
    if (val == 0) return true;
    int bs = boxSize(n);
    for (int c = 0; c < n; ++c)
        if (g[row * n + c] == val) return false;
    for (int r = 0; r < n; ++r)
        if (g[r * n + col] == val) return false;
    int br = row / bs, bc = col / bs;
    for (int r = br * bs; r < (br + 1) * bs; ++r)
        for (int c = bc * bs; c < (bc + 1) * bs; ++c)
            if (g[r * n + c] == val) return false;
    return true;
}

static bool sudokuCompleteValid(const std::vector<uint8_t>& g, int n) {
    int bs = boxSize(n);
    for (int r = 0; r < n; ++r) {
        std::vector<bool> seen(256, false);
        for (int c = 0; c < n; ++c) {
            uint8_t v = g[r * n + c];
            if (v == 0) continue;
            if (seen[v]) return false;
            seen[v] = true;
        }
    }
    for (int c = 0; c < n; ++c) {
        std::vector<bool> seen(256, false);
        for (int r = 0; r < n; ++r) {
            uint8_t v = g[r * n + c];
            if (v == 0) continue;
            if (seen[v]) return false;
            seen[v] = true;
        }
    }
    for (int br = 0; br < bs; ++br)
        for (int bc = 0; bc < bs; ++bc) {
            std::vector<bool> seen(256, false);
            for (int r = br * bs; r < (br + 1) * bs; ++r)
                for (int c = bc * bs; c < (bc + 1) * bs; ++c) {
                    uint8_t v = g[r * n + c];
                    if (v == 0) continue;
                    if (seen[v]) return false;
                    seen[v] = true;
                }
        }
    return true;
}

static long g_nodes = 0;
static const long g_nodeCap = 5000000;

static bool sudokuSolve(std::vector<uint8_t>& g, const std::vector<uint8_t>& fixed,
                        int n, int start) {
    g_nodes++;
    if (g_nodes > g_nodeCap) return false;
    int idx = start;
    while (idx < n * n && fixed[idx]) ++idx;
    if (idx >= n * n) return true;
    int row = idx / n, col = idx % n;
    for (int v = 1; v <= 255; ++v) {
        uint8_t val = static_cast<uint8_t>(v);
        if (sudokuValid(g, n, row, col, val)) {
            g[idx] = val;
            if (sudokuSolve(g, fixed, n, idx + 1)) return true;
            g[idx] = 0;
        }
    }
    return false;
}

// ============================================================================
// XOR helper
// ============================================================================
static std::vector<uint8_t> xorCrypt(const std::vector<uint8_t>& data,
                                     const std::string& key) {
    std::vector<uint8_t> out = data;
    if (key.empty()) return out;
    for (size_t i = 0; i < data.size(); ++i)
        out[i] ^= static_cast<uint8_t>(key[i % key.size()]);
    return out;
}

// ============================================================================
// PBKDF2-HMAC-SHA256 key derivation
// ============================================================================
std::string deriveKey(const std::string& passphrase,
                      const std::string& salt,
                      int iterations) {
    unsigned char out[32];
    if (PKCS5_PBKDF2_HMAC(passphrase.c_str(), static_cast<int>(passphrase.size()),
                          reinterpret_cast<const unsigned char*>(salt.data()),
                          static_cast<int>(salt.size()), iterations,
                          EVP_sha256(), 32, out) != 1) {
        return "";
    }
    return std::string(reinterpret_cast<const char*>(out), 32);
}

// ============================================================================
// zlib compression / decompression
// ============================================================================
std::vector<uint8_t> compressData(const std::vector<uint8_t>& data) {
    uLongf destLen = compressBound(static_cast<uLong>(data.size()));
    std::vector<uint8_t> out(destLen);
    int rc = compress2(out.data(), &destLen, data.data(),
                       static_cast<uLong>(data.size()), Z_DEFAULT_COMPRESSION);
    if (rc != Z_OK) return {};
    out.resize(static_cast<size_t>(destLen));
    return out;
}

std::vector<uint8_t> decompressData(const std::vector<uint8_t>& data) {
    // Try growing buffer sizes
    uLongf destLen = data.size() * 4 + 16;
    std::vector<uint8_t> out(destLen);
    int rc = uncompress(out.data(), &destLen, data.data(),
                        static_cast<uLong>(data.size()));
    if (rc == Z_BUF_ERROR) {
        destLen = data.size() * 16 + 1024;
        out.resize(destLen);
        rc = uncompress(out.data(), &destLen, data.data(),
                        static_cast<uLong>(data.size()));
    }
    if (rc != Z_OK) return {};
    out.resize(static_cast<size_t>(destLen));
    return out;
}

// ============================================================================
// Page header helpers (big-endian)
// Header: 10 bytes
//   [0-1]   page index (uint16 BE)
//   [2-3]   total pages (uint16 BE)
//   [4-5]   payload data length on this page (uint16 BE)
//   [6-9]   CRC32 of payload data (uint32 BE)
// ============================================================================
static constexpr int PAGE_HEADER_SIZE = 10;
static constexpr char BATCH_MAGIC[5] = {'B', 'A', 'T', 'C', 'H'};

static void putBE16(uint8_t* p, uint32_t v) { p[0] = static_cast<uint8_t>(v >> 8); p[1] = static_cast<uint8_t>(v & 0xFF); }
static void putBE32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v >> 24); p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 8) & 0xFF); p[3] = static_cast<uint8_t>(v & 0xFF);
}
static uint32_t getBE16(const uint8_t* p) { return (static_cast<uint32_t>(p[0]) << 8) | p[1]; }
static uint32_t getBE32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

// ============================================================================
// Auto ECC selection
// ============================================================================
static int selectECC(int n, size_t payloadSize) {
    int totalCells = n * n;
    int headerAndSym = PAGE_HEADER_SIZE;
    int usable = totalCells - headerAndSym;
    if (usable <= 0) return 2;
    double ratio = static_cast<double>(payloadSize) / static_cast<double>(usable);
    if (ratio > 0.60) return 3;
    if (ratio > 0.30) return 2;
    if (ratio > 0.10) return 1;
    return 0;
}

// ============================================================================
// Encode single data blob into pages
// ============================================================================
static std::vector<Code> encodePages(const std::vector<uint8_t>& data,
                                     const Params& p) {
    gf::init();
    int n = p.n;
    int totalCells = n * n;

    int nsym = p.autoEcc ? selectECC(n, data.size()) : p.eccLevel;
    if (nsym < 0) nsym = 0;
    if (nsym > 3) nsym = 3;

    int maxPayload = totalCells - PAGE_HEADER_SIZE - nsym;
    if (maxPayload <= 0)
        throw std::runtime_error("grid too small for page header + ECC");

    int totalPages = static_cast<int>((data.size() + maxPayload - 1) / maxPayload);
    if (totalPages < 1) totalPages = 1;

    // Resolve key: passphrase -> PBKDF2, else raw key
    std::string effectiveKey;
    if (!p.passphrase.empty()) {
        std::string salt = "SudokuCode";
        salt += static_cast<char>((n >> 8) & 0xFF);
        salt += static_cast<char>(n & 0xFF);
        effectiveKey = deriveKey(p.passphrase, salt, 10000);
        if (effectiveKey.empty())
            throw std::runtime_error("key derivation failed");
    } else {
        effectiveKey = p.key;
    }

    std::vector<Code> pages;
    pages.reserve(static_cast<size_t>(totalPages));

    for (int pg = 0; pg < totalPages; ++pg) {
        size_t offset = static_cast<size_t>(pg) * maxPayload;
        size_t chunkSize = std::min(static_cast<size_t>(maxPayload), data.size() - offset);
        std::vector<uint8_t> chunk(data.begin() + offset,
                                   data.begin() + offset + chunkSize);

        uint32_t c = crc32(chunk.data(), chunk.size());

        // Build page header + payload
        std::vector<uint8_t> header(PAGE_HEADER_SIZE);
        putBE16(header.data(), static_cast<uint32_t>(pg));
        putBE16(header.data() + 2, static_cast<uint32_t>(totalPages));
        putBE16(header.data() + 4, static_cast<uint32_t>(chunkSize));
        putBE32(header.data() + 6, c);

        std::vector<uint8_t> payload = header;
        payload.insert(payload.end(), chunk.begin(), chunk.end());

        // optional compress before encrypt
        if (p.compress) {
            auto compressed = compressData(payload);
            if (!compressed.empty())
                payload = std::move(compressed);
        }

        // optional encrypt
        if (p.encrypt && !effectiveKey.empty())
            payload = xorCrypt(payload, effectiveKey);

        int k = static_cast<int>(payload.size());
        if (k + nsym > totalCells)
            throw std::runtime_error("payload too large for grid with ECC");

        // RS systematic encode
        std::vector<uint8_t> codeword;
        if (nsym == 0) {
            codeword = payload;
        } else {
            std::vector<uint8_t> gen(1, 1);
            for (int i = 0; i < nsym; ++i) {
                uint8_t root = gf::pow(2, i);
                std::vector<uint8_t> next(gen.size() + 1, 0);
                for (size_t j = 0; j < gen.size(); ++j) {
                    next[j] ^= gen[j];
                    next[j + 1] ^= gf::mul(gen[j], root);
                }
                gen.swap(next);
            }
            std::vector<uint8_t> temp(payload.size() + nsym, 0);
            std::copy(payload.begin(), payload.end(), temp.begin());
            for (size_t i = 0; i < payload.size(); ++i) {
                uint8_t coef = temp[i];
                if (coef != 0) {
                    for (size_t j = 1; j < gen.size(); ++j)
                        temp[i + j] ^= gf::mul(gen[j], coef);
                }
            }
            std::vector<uint8_t> parity(temp.end() - nsym, temp.end());
            codeword = payload;
            codeword.insert(codeword.end(), parity.begin(), parity.end());
        }

        // Place codeword in grid row-major; must be sudoku-conflict-free
        std::vector<uint8_t> g(totalCells, 0);
        std::vector<uint8_t> fixed(totalCells, 0);
        for (int i = 0; i < k + nsym; ++i) {
            g[i] = codeword[i];
            fixed[i] = 1;
        }

        if (!sudokuCompleteValid(g, n)) {
            throw std::runtime_error("data conflicts with sudoku constraints "
                                     "(duplicate in row/col/box)");
        }
        g_nodes = 0;
        if (!sudokuSolve(g, fixed, n, 0)) {
            throw std::runtime_error("failed to solve sudoku grid");
        }

        Code code;
        code.n = n;
        code.grid = g;
        code.mask = fixed;
        code.length = static_cast<uint32_t>(chunkSize);
        code.page = static_cast<uint32_t>(pg);
        code.totalPages = static_cast<uint32_t>(totalPages);
        code.crc = c;
        pages.push_back(std::move(code));
    }
    return pages;
}

// ============================================================================
// encode (public)
// ============================================================================
std::vector<Code> encode(const std::vector<uint8_t>& data, const Params& p) {
    return encodePages(data, p);
}

// ============================================================================
// Decode single page
// ============================================================================
static std::vector<uint8_t> decodePage(const Code& code, const Params& p,
                                       std::string& effectiveKey) {
    int n = code.n;
    int totalCells = n * n;
    if (static_cast<int>(code.grid.size()) != totalCells)
        throw std::runtime_error("bad grid size");
    if (!sudokuCompleteValid(code.grid, n))
        throw std::runtime_error("invalid sudoku grid");

    int nsym = p.autoEcc ? selectECC(n, code.length) : p.eccLevel;
    if (nsym < 0) nsym = 0;
    if (nsym > 3) nsym = 3;

    int k = PAGE_HEADER_SIZE + static_cast<int>(code.length);
    if (k + nsym > totalCells) throw std::runtime_error("length exceeds grid");

    std::vector<uint8_t> r(code.grid.begin(),
                           code.grid.begin() + k + nsym);

    std::vector<uint8_t> payload = rsDecode(r, nsym, k);
    if (payload.empty())
        throw std::runtime_error("Reed-Solomon decode failed (too many errors)");

    // optional decrypt
    if (p.encrypt && !effectiveKey.empty())
        payload = xorCrypt(payload, effectiveKey);

    // optional decompress
    if (p.compress) {
        auto decompressed = decompressData(payload);
        if (decompressed.empty())
            throw std::runtime_error("decompression failed");
        payload = std::move(decompressed);
    }

    if (payload.size() < PAGE_HEADER_SIZE)
        throw std::runtime_error("page header too short");

    uint32_t pg = getBE16(payload.data());
    uint32_t total = getBE16(payload.data() + 2);
    uint32_t dataLen = getBE16(payload.data() + 4);
    uint32_t crc = getBE32(payload.data() + 6);

    if (pg != code.page || total != code.totalPages)
        throw std::runtime_error("page index mismatch");
    if (dataLen != code.length)
        throw std::runtime_error("page length mismatch");

    std::vector<uint8_t> chunk(payload.begin() + PAGE_HEADER_SIZE,
                               payload.begin() + PAGE_HEADER_SIZE + dataLen);
    uint32_t calc = crc32(chunk.data(), chunk.size());
    if (calc != crc)
        throw std::runtime_error("CRC32 mismatch; page data corrupted");

    return chunk;
}

// ============================================================================
// decode (public)
// ============================================================================
std::vector<uint8_t> decode(const std::vector<Code>& pages, const Params& p) {
    gf::init();
    if (pages.empty())
        throw std::runtime_error("no pages to decode");

    uint32_t totalPages = pages[0].totalPages;
    if (pages.size() != totalPages)
        throw std::runtime_error("page count mismatch");

    // Resolve key
    std::string effectiveKey;
    if (!p.passphrase.empty()) {
        std::string salt = "SudokuCode";
        salt += static_cast<char>((pages[0].n >> 8) & 0xFF);
        salt += static_cast<char>(pages[0].n & 0xFF);
        effectiveKey = deriveKey(p.passphrase, salt, 10000);
        if (effectiveKey.empty())
            throw std::runtime_error("key derivation failed");
    } else {
        effectiveKey = p.key;
    }

    std::vector<uint8_t> data;
    for (const auto& code : pages) {
        auto chunk = decodePage(code, p, effectiveKey);
        data.insert(data.end(), chunk.begin(), chunk.end());
    }
    return data;
}

// ============================================================================
// verify (public)
// ============================================================================
bool verify(const std::vector<Code>& pages, const Params& p) {
    gf::init();
    if (pages.empty()) return false;
    uint32_t totalPages = pages[0].totalPages;
    if (pages.size() != totalPages) return false;

    std::string effectiveKey;
    if (!p.passphrase.empty()) {
        std::string salt = "SudokuCode";
        salt += static_cast<char>((pages[0].n >> 8) & 0xFF);
        salt += static_cast<char>(pages[0].n & 0xFF);
        effectiveKey = deriveKey(p.passphrase, salt, 10000);
        if (effectiveKey.empty()) return false;
    } else {
        effectiveKey = p.key;
    }

    for (const auto& code : pages) {
        int n = code.n;
        int totalCells = n * n;
        if (static_cast<int>(code.grid.size()) != totalCells) return false;
        if (!sudokuCompleteValid(code.grid, n)) return false;

        int nsym = p.autoEcc ? selectECC(n, code.length) : p.eccLevel;
        if (nsym < 0) nsym = 0;
        if (nsym > 3) nsym = 3;

        int k = PAGE_HEADER_SIZE + static_cast<int>(code.length);
        if (k + nsym > totalCells) return false;

        std::vector<uint8_t> r(code.grid.begin(),
                               code.grid.begin() + k + nsym);
        std::vector<uint8_t> payload = rsDecode(r, nsym, k);
        if (payload.empty()) return false;

        if (p.encrypt && !effectiveKey.empty())
            payload = xorCrypt(payload, effectiveKey);
        if (p.compress) {
            auto decompressed = decompressData(payload);
            if (decompressed.empty()) return false;
            payload = std::move(decompressed);
        }

        if (payload.size() < PAGE_HEADER_SIZE) return false;
        uint32_t crc = getBE32(payload.data() + 6);
        uint32_t dataLen = getBE16(payload.data() + 4);
        if (dataLen != code.length) return false;
        std::vector<uint8_t> chunk(payload.begin() + PAGE_HEADER_SIZE,
                                   payload.begin() + PAGE_HEADER_SIZE + dataLen);
        if (crc32(chunk.data(), chunk.size()) != crc) return false;
    }
    return true;
}

// ============================================================================
// Batch mode
// Index page format:
//   "BATCH" (5 bytes)
//   file count (uint16 BE)
//   per file: nameLen (uint8), name bytes, dataSize (uint32 BE)
//   then concatenated file data across all pages
// ============================================================================
std::vector<Code> encodeBatch(const std::vector<BatchEntry>& files,
                              const Params& p) {
    // Build index + data stream
    std::vector<uint8_t> stream;
    // magic
    stream.insert(stream.end(), BATCH_MAGIC, BATCH_MAGIC + 5);
    // file count
    uint16_t fileCount = static_cast<uint16_t>(files.size());
    stream.push_back(static_cast<uint8_t>(fileCount >> 8));
    stream.push_back(static_cast<uint8_t>(fileCount & 0xFF));
    // per-file metadata
    for (const auto& f : files) {
        stream.push_back(static_cast<uint8_t>(f.filename.size()));
        stream.insert(stream.end(), f.filename.begin(), f.filename.end());
        uint32_t sz = static_cast<uint32_t>(f.data.size());
        stream.push_back(static_cast<uint8_t>(sz >> 24));
        stream.push_back(static_cast<uint8_t>((sz >> 16) & 0xFF));
        stream.push_back(static_cast<uint8_t>((sz >> 8) & 0xFF));
        stream.push_back(static_cast<uint8_t>(sz & 0xFF));
    }
    // file data
    for (const auto& f : files)
        stream.insert(stream.end(), f.data.begin(), f.data.end());

    return encodePages(stream, p);
}

std::vector<BatchEntry> decodeBatch(const std::vector<Code>& pages,
                                    const Params& p) {
    auto stream = decode(pages, p);
    if (stream.size() < 6) throw std::runtime_error("batch stream too short");
    if (std::memcmp(stream.data(), BATCH_MAGIC, 5) != 0)
        throw std::runtime_error("not a batch image");

    size_t idx = 5;
    uint16_t fileCount = (static_cast<uint16_t>(stream[idx]) << 8) | stream[idx + 1];
    idx += 2;

    std::vector<BatchEntry> files;
    files.reserve(fileCount);

    // Read metadata first
    std::vector<std::string> names;
    std::vector<uint32_t> sizes;
    names.reserve(fileCount);
    sizes.reserve(fileCount);
    for (uint16_t i = 0; i < fileCount; ++i) {
        if (idx >= stream.size()) throw std::runtime_error("truncated batch index");
        uint8_t nameLen = stream[idx++];
        names.emplace_back(stream.begin() + idx, stream.begin() + idx + nameLen);
        idx += nameLen;
        if (idx + 4 > stream.size()) throw std::runtime_error("truncated batch size");
        uint32_t sz = (static_cast<uint32_t>(stream[idx]) << 24) |
                      (static_cast<uint32_t>(stream[idx + 1]) << 16) |
                      (static_cast<uint32_t>(stream[idx + 2]) << 8) |
                      stream[idx + 3];
        idx += 4;
        sizes.push_back(sz);
    }

    // Extract data
    for (uint16_t i = 0; i < fileCount; ++i) {
        BatchEntry e;
        e.filename = names[i];
        if (idx + sizes[i] > stream.size())
            throw std::runtime_error("truncated batch data");
        e.data.assign(stream.begin() + idx, stream.begin() + idx + sizes[i]);
        idx += sizes[i];
        files.push_back(std::move(e));
    }
    return files;
}

// ============================================================================
// BMP (24-bit, no external deps)
// Finder pattern (7x7 modules, QR-style):
//   B B B B B B B
//   B W W W W W B
//   B W B B B W B
//   B W B B B W B
//   B W B B B W B
//   B W W W W W B
//   B B B B B B B
// ============================================================================
#pragma pack(push,1)
struct BMPFileHeader { uint16_t type; uint32_t size; uint16_t reserved1; uint16_t reserved2; uint32_t offBits; };
struct BMPInfoHeader { uint32_t size; int32_t width; int32_t height; uint16_t planes; uint16_t bitCount; uint32_t compression; uint32_t sizeImage; int32_t xPels; int32_t yPels; uint32_t clrUsed; uint32_t clrImportant; };
#pragma pack(pop)

static int bmpRowStride(int w) { return (w * 3 + 3) & ~3; }

static constexpr int FINDER_SIZE = 7; // modules
static const uint8_t FINDER_PATTERN[FINDER_SIZE][FINDER_SIZE] = {
    {1,1,1,1,1,1,1},
    {1,0,0,0,0,0,1},
    {1,0,1,1,1,0,1},
    {1,0,1,1,1,0,1},
    {1,0,1,1,1,0,1},
    {1,0,0,0,0,0,1},
    {1,1,1,1,1,1,1},
};

static int finderPixel(int v) { return v ? 0 : 255; } // 1=black, 0=white

// Render one page grid into pixel buffer (bottom-up BMP coords).
static void renderPagePixels(const Code& c, int cellSize, int border,
                             std::vector<uint8_t>& img, int w, int h,
                             int pageYOffset, bool useFinder,
                             int finderMargin) {
    int n = c.n;
    int cell = cellSize > 0 ? cellSize : 20;
    int cw = cell + border;
    int stride = bmpRowStride(w);

    for (int r = 0; r < n; ++r) {
        for (int col = 0; col < n; ++col) {
            uint8_t v = c.grid[r * n + col];
            int x0 = border + col * cw;
            int y0 = border + r * cw + pageYOffset;
            for (int dy = 0; dy < cell; ++dy) {
                uint8_t* row = &img[(h - 1 - (y0 + dy)) * stride + x0 * 3];
                for (int dx = 0; dx < cell; ++dx) {
                    row[dx*3 + 0] = v;
                    row[dx*3 + 1] = v;
                    row[dx*3 + 2] = v;
                }
            }
        }
    }
    // grid lines (black)
    for (int r = 0; r <= n; ++r) {
        int y = border + r * cw + pageYOffset;
        for (int x = 0; x < w; ++x) {
            uint8_t* row = &img[(h - 1 - y) * stride + x * 3];
            row[0]=row[1]=row[2]=0;
        }
    }
    for (int col = 0; col <= n; ++col) {
        int x = border + col * cw;
        for (int y = 0; y < n * cw + border; ++y) {
            int yy = y + pageYOffset;
            if (yy >= h) break;
            uint8_t* row = &img[(h - 1 - yy) * stride + x * 3];
            row[0]=row[1]=row[2]=0;
        }
    }

    if (useFinder) {
        int fw = FINDER_SIZE * cell;
        int fm = finderMargin;
        // Top-left finder
        for (int r = 0; r < FINDER_SIZE; ++r) {
            for (int col = 0; col < FINDER_SIZE; ++col) {
                uint8_t fill = static_cast<uint8_t>(finderPixel(FINDER_PATTERN[r][col]));
                int x0 = border + fm + col * cell;
                int y0 = border + fm + r * cell + pageYOffset;
                for (int dy = 0; dy < cell; ++dy) {
                    uint8_t* row = &img[(h - 1 - (y0 + dy)) * stride + x0 * 3];
                    for (int dx = 0; dx < cell; ++dx) {
                        row[dx*3+0] = fill; row[dx*3+1] = fill; row[dx*3+2] = fill;
                    }
                }
            }
        }
        // Top-right finder
        for (int r = 0; r < FINDER_SIZE; ++r) {
            for (int col = 0; col < FINDER_SIZE; ++col) {
                uint8_t fill = static_cast<uint8_t>(finderPixel(FINDER_PATTERN[r][col]));
                int x0 = w - border - fm - (FINDER_SIZE - col) * cell;
                int y0 = border + fm + r * cell + pageYOffset;
                for (int dy = 0; dy < cell; ++dy) {
                    uint8_t* row = &img[(h - 1 - (y0 + dy)) * stride + x0 * 3];
                    for (int dx = 0; dx < cell; ++dx) {
                        row[dx*3+0] = fill; row[dx*3+1] = fill; row[dx*3+2] = fill;
                    }
                }
            }
        }
        // Bottom-left finder
        for (int r = 0; r < FINDER_SIZE; ++r) {
            for (int col = 0; col < FINDER_SIZE; ++col) {
                uint8_t fill = static_cast<uint8_t>(finderPixel(FINDER_PATTERN[r][col]));
                int x0 = border + fm + col * cell;
                int y0 = pageYOffset + border + fm + (n * cw + border) - FINDER_SIZE * cell - r * cell - cell;
                for (int dy = 0; dy < cell; ++dy) {
                    uint8_t* row = &img[(h - 1 - (y0 + dy)) * stride + x0 * 3];
                    for (int dx = 0; dx < cell; ++dx) {
                        row[dx*3+0] = fill; row[dx*3+1] = fill; row[dx*3+2] = fill;
                    }
                }
            }
        }
    }
}

bool writeBMP(const std::string& path, const std::vector<Code>& pages,
              int cellSize, bool useFinder) {
    if (pages.empty()) return false;
    int n = pages[0].n;
    int cell = cellSize > 0 ? cellSize : 20;
    int border = 2;
    int cw = cell + border;
    int pageW = n * cw + border;
    int pageH = n * cw + border;
    int gap = 4;
    int totalPages = static_cast<int>(pages.size());
    int finderMargin = useFinder ? cell : 0;
    int w = pageW + (useFinder ? 2 * finderMargin : 0);
    int h = totalPages * pageH + (totalPages - 1) * gap +
            (useFinder ? 2 * finderMargin : 0);
    int stride = bmpRowStride(w);

    std::vector<uint8_t> img(stride * h, 255);

    int yOffset = (useFinder ? finderMargin : 0);
    for (const auto& c : pages) {
        renderPagePixels(c, cell, border, img, w, h, yOffset, useFinder, finderMargin);
        yOffset += pageH + gap;
    }

    BMPFileHeader fh; fh.type = 0x4D42; fh.reserved1 = 0; fh.reserved2 = 0;
    fh.offBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
    fh.size = fh.offBits + static_cast<uint32_t>(stride * h);
    BMPInfoHeader ih; ih.size = sizeof(BMPInfoHeader); ih.width = w; ih.height = h;
    ih.planes = 1; ih.bitCount = 24; ih.compression = 0;
    ih.sizeImage = static_cast<uint32_t>(stride * h);
    ih.xPels = ih.yPels = 0; ih.clrUsed = ih.clrImportant = 0;

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    ofs.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
    ofs.write(reinterpret_cast<const char*>(&ih), sizeof(ih));
    ofs.write(reinterpret_cast<const char*>(img.data()), img.size());
    return ofs.good();
}

// Read grid from pixel region; returns false if finders present but not found.
static bool readPageFromPixels(const uint8_t* px, int w, int h, int stride,
                               int pageYOffset, int cellSize, int border,
                               bool useFinder, int finderMargin, Code& out) {
    int cell = cellSize > 0 ? cellSize : 20;
    int cw = cell + border;
    int n = (w - 2 * finderMargin - 2 * border) / cw;
    if (n != 4 && n != 9 && n != 16) return false;
    out.n = n;
    out.grid.assign(n * n, 0);
    out.mask.assign(n * n, 1);

    int xBase = border + finderMargin;
    int yBase = pageYOffset + border + finderMargin;

    for (int r = 0; r < n; ++r) {
        for (int col = 0; col < n; ++col) {
            int cx = xBase + col * cw + cell / 2;
            int cy = yBase + r * cw + cell / 2;
            if (cx < 0 || cx >= w || cy < 0 || cy >= h) return false;
            uint8_t v = px[(h - 1 - cy) * stride + cx * 3 + 2]; // R channel
            out.grid[r * n + col] = v;
        }
    }
    out.length = static_cast<uint32_t>(n * n);
    out.crc = 0;
    return true;
}

bool readBMP(const std::string& path, std::vector<Code>& pages,
             int cellSize, bool useFinder) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    BMPFileHeader fh; BMPInfoHeader ih;
    ifs.read(reinterpret_cast<char*>(&fh), sizeof(fh));
    ifs.read(reinterpret_cast<char*>(&ih), sizeof(ih));
    if (fh.type != 0x4D42 || ih.bitCount != 24) return false;
    int w = ih.width, h = ih.height;
    int stride = bmpRowStride(w);
    int cell = cellSize > 0 ? cellSize : 20;
    int border = 2;
    int finderMargin = useFinder ? cell : 0;

    std::vector<uint8_t> px(stride * h);
    ifs.seekg(fh.offBits);
    ifs.read(reinterpret_cast<char*>(px.data()), px.size());

    int cw = cell + border;
    int pageH = 0;
    int n = 0;
    // Deduce n from width
    if (useFinder) {
        n = (w - 2 * finderMargin - 2 * border) / cw;
    } else {
        n = (w - 2 * border) / cw;
    }
    if (n != 4 && n != 9 && n != 16) return false;
    pageH = n * cw + border;

    int finderTop = useFinder ? finderMargin : 0;
    int usableH = h - 2 * finderTop;
    int gap = 4;
    int pagePitch = pageH + gap;
    if (usableH % pagePitch != 0) return false;
    int totalPages = usableH / pagePitch;
    if (totalPages < 1) return false;

    pages.clear();
    pages.reserve(static_cast<size_t>(totalPages));
    int yOffset = finderTop;
    for (int i = 0; i < totalPages; ++i) {
        Code c;
        if (!readPageFromPixels(px.data(), w, h, stride, yOffset, cell, border,
                                useFinder, finderMargin, c))
            return false;
        pages.push_back(std::move(c));
        yOffset += pagePitch;
    }
    return true;
}

// ============================================================================
// Steganography (LSB hide/extract in ordinary BMP photo)
// ============================================================================
static bool readCoverBMP(const std::string& path,
                         std::vector<uint8_t>& px, int& w, int& h, int& stride) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    BMPFileHeader fh; BMPInfoHeader ih;
    ifs.read(reinterpret_cast<char*>(&fh), sizeof(fh));
    ifs.read(reinterpret_cast<char*>(&ih), sizeof(ih));
    if (fh.type != 0x4D42 || ih.bitCount != 24) return false;
    w = ih.width; h = ih.height;
    stride = bmpRowStride(w);
    px.resize(stride * h);
    ifs.seekg(fh.offBits);
    ifs.read(reinterpret_cast<char*>(px.data()), px.size());
    return true;
}

static bool writeCoverBMP(const std::string& path,
                          const std::vector<uint8_t>& px, int w, int h,
                          int stride, const BMPFileHeader& fh,
                          const BMPInfoHeader& ih) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    ofs.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
    ofs.write(reinterpret_cast<const char*>(&ih), sizeof(ih));
    ofs.write(reinterpret_cast<const char*>(px.data()), px.size());
    return ofs.good();
}

bool hideInImage(const std::string& coverPath, const std::string& outPath,
                 const std::vector<Code>& pages, int cellSize) {
    // Serialize pages into a byte stream: n(totalPages), then per page:
    //   page, totalPages, length, crc, grid bytes
    std::vector<uint8_t> payload;
    uint32_t totalPages = static_cast<uint32_t>(pages.size());
    payload.push_back(static_cast<uint8_t>(totalPages >> 24));
    payload.push_back(static_cast<uint8_t>((totalPages >> 16) & 0xFF));
    payload.push_back(static_cast<uint8_t>((totalPages >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(totalPages & 0xFF));
    for (const auto& c : pages) {
        payload.push_back(static_cast<uint8_t>(c.page >> 24));
        payload.push_back(static_cast<uint8_t>((c.page >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((c.page >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(c.page & 0xFF));
        payload.push_back(static_cast<uint8_t>(c.totalPages >> 24));
        payload.push_back(static_cast<uint8_t>((c.totalPages >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((c.totalPages >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(c.totalPages & 0xFF));
        payload.push_back(static_cast<uint8_t>(c.length >> 24));
        payload.push_back(static_cast<uint8_t>((c.length >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((c.length >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(c.length & 0xFF));
        payload.push_back(static_cast<uint8_t>(c.crc >> 24));
        payload.push_back(static_cast<uint8_t>((c.crc >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((c.crc >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(c.crc & 0xFF));
        payload.insert(payload.end(), c.grid.begin(), c.grid.end());
        payload.insert(payload.end(), c.mask.begin(), c.mask.end());
    }

    int w, h, stride;
    std::vector<uint8_t> px;
    BMPFileHeader fh; BMPInfoHeader ih;
    std::ifstream ifs(coverPath, std::ios::binary);
    if (!ifs) return false;
    ifs.read(reinterpret_cast<char*>(&fh), sizeof(fh));
    ifs.read(reinterpret_cast<char*>(&ih), sizeof(ih));
    if (fh.type != 0x4D42 || ih.bitCount != 24) return false;
    w = ih.width; h = ih.height;
    stride = bmpRowStride(w);
    px.resize(stride * h);
    ifs.seekg(fh.offBits);
    ifs.read(reinterpret_cast<char*>(px.data()), px.size());
    ifs.close();

    size_t neededBits = payload.size() * 8 + 32; // + length prefix
    size_t availableBits = static_cast<size_t>(stride * h) * 3; // 3 channels per pixel byte
    if (neededBits > availableBits) return false;

    // Write length prefix (32 bits) then payload bits into LSBs
    size_t bitIdx = 0;
    auto writeBit = [&](bool b) {
        size_t byteIdx = bitIdx / 3;
        size_t chan = bitIdx % 3;
        if (byteIdx >= px.size()) return;
        if (b) px[byteIdx] |= (1u << chan);
        else   px[byteIdx] &= ~(1u << chan);
        bitIdx++;
    };
    // We need a flat bit index across all bytes/channels. Let's restructure:
    // Use pixel byte index and channel 0,1,2 sequentially.
    bitIdx = 0;
    auto getBit = [&](const std::vector<uint8_t>& data, size_t bit) -> bool {
        size_t byte = bit / 8;
        unsigned b = bit % 8;
        return (data[byte] >> b) & 1u;
    };
    // Write 32-bit payload length
    for (int i = 0; i < 32; ++i) {
        bool b = getBit(std::vector<uint8_t>(
            {static_cast<uint8_t>((payload.size() >> 24) & 0xFF),
             static_cast<uint8_t>((payload.size() >> 16) & 0xFF),
             static_cast<uint8_t>((payload.size() >> 8) & 0xFF),
             static_cast<uint8_t>(payload.size() & 0xFF)}), i);
        size_t byteIdx = bitIdx / 3;
        size_t chan = bitIdx % 3;
        if (byteIdx >= px.size()) return false;
        if (b) px[byteIdx] |= (1u << chan); else px[byteIdx] &= ~(1u << chan);
        bitIdx++;
    }
    // Write payload
    for (size_t i = 0; i < payload.size(); ++i) {
        for (int b = 0; b < 8; ++b) {
            bool bit = (payload[i] >> b) & 1u;
            size_t byteIdx = bitIdx / 3;
            size_t chan = bitIdx % 3;
            if (byteIdx >= px.size()) return false;
            if (bit) px[byteIdx] |= (1u << chan); else px[byteIdx] &= ~(1u << chan);
            bitIdx++;
        }
    }

    return writeCoverBMP(outPath, px, w, h, stride, fh, ih);
}

bool extractFromImage(const std::string& stegoPath,
                      std::vector<Code>& pages, int cellSize) {
    int w, h, stride;
    std::vector<uint8_t> px;
    BMPFileHeader fh; BMPInfoHeader ih;
    std::ifstream ifs(stegoPath, std::ios::binary);
    if (!ifs) return false;
    ifs.read(reinterpret_cast<char*>(&fh), sizeof(fh));
    ifs.read(reinterpret_cast<char*>(&ih), sizeof(ih));
    if (fh.type != 0x4D42 || ih.bitCount != 24) return false;
    w = ih.width; h = ih.height;
    stride = bmpRowStride(w);
    px.resize(stride * h);
    ifs.seekg(fh.offBits);
    ifs.read(reinterpret_cast<char*>(px.data()), px.size());
    ifs.close();

    size_t bitIdx = 0;
    auto readBit = [&]() -> bool {
        size_t byteIdx = bitIdx / 3;
        size_t chan = bitIdx % 3;
        if (byteIdx >= px.size()) return false;
        bool b = (px[byteIdx] >> chan) & 1u;
        bitIdx++;
        return b;
    };
    auto readByte = [&](uint8_t& out) -> bool {
        out = 0;
        for (int b = 0; b < 8; ++b) {
            if (!readBit()) return false;
            out |= (static_cast<uint8_t>(readBit() ? 1 : 0) << b);
        }
        return true;
    };
    // We need to re-read bits properly. The above readBit consumes one bit per call.
    // Let me rewrite readByte to consume 8 bits.
    bitIdx = 0;
    auto readBit2 = [&]() -> bool {
        size_t byteIdx = bitIdx / 3;
        size_t chan = bitIdx % 3;
        if (byteIdx >= px.size()) return false;
        bool b = (px[byteIdx] >> chan) & 1u;
        bitIdx++;
        return b;
    };
    auto readByte2 = [&](uint8_t& out) -> bool {
        out = 0;
        for (int b = 0; b < 8; ++b) {
            bool b2 = readBit2();
            out |= (static_cast<uint8_t>(b2 ? 1 : 0) << b);
        }
        return true;
    };

    // Read 32-bit payload length
    uint32_t payloadLen = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t b;
        if (!readByte2(b)) return false;
        payloadLen |= (static_cast<uint32_t>(b) << (24 - i * 8));
    }

    size_t neededBits = (payloadLen * 8 + 32) * 3 / 2; // rough
    if (bitIdx + payloadLen * 8 > px.size() * 3) return false;

    std::vector<uint8_t> payload(payloadLen);
    for (uint32_t i = 0; i < payloadLen; ++i) {
        if (!readByte2(payload[i])) return false;
    }

    // Deserialize pages
    if (payload.size() < 4) return false;
    uint32_t totalPages = (static_cast<uint32_t>(payload[0]) << 24) |
                          (static_cast<uint32_t>(payload[1]) << 16) |
                          (static_cast<uint32_t>(payload[2]) << 8) |
                          payload[3];
    size_t idx = 4;
    pages.clear();
    pages.reserve(totalPages);
    for (uint32_t i = 0; i < totalPages; ++i) {
        Code c;
        if (idx + 24 > payload.size()) return false;
        c.page = (static_cast<uint32_t>(payload[idx]) << 24) |
                 (static_cast<uint32_t>(payload[idx+1]) << 16) |
                 (static_cast<uint32_t>(payload[idx+2]) << 8) |
                 payload[idx+3]; idx += 4;
        c.totalPages = (static_cast<uint32_t>(payload[idx]) << 24) |
                       (static_cast<uint32_t>(payload[idx+1]) << 16) |
                       (static_cast<uint32_t>(payload[idx+2]) << 8) |
                       payload[idx+3]; idx += 4;
        c.length = (static_cast<uint32_t>(payload[idx]) << 24) |
                   (static_cast<uint32_t>(payload[idx+1]) << 16) |
                   (static_cast<uint32_t>(payload[idx+2]) << 8) |
                   payload[idx+3]; idx += 4;
        c.crc = (static_cast<uint32_t>(payload[idx]) << 24) |
                (static_cast<uint32_t>(payload[idx+1]) << 16) |
                (static_cast<uint32_t>(payload[idx+2]) << 8) |
                payload[idx+3]; idx += 4;
        // Determine n from grid size remaining - we need n. Store n implicitly:
        // Try each n
        bool foundN = false;
        for (int candN : {4, 9, 16}) {
            size_t gridSize = static_cast<size_t>(candN) * candN;
            if (idx + 2 * gridSize <= payload.size()) {
                c.n = candN;
                c.grid.assign(payload.begin() + idx, payload.begin() + idx + gridSize);
                idx += gridSize;
                c.mask.assign(payload.begin() + idx, payload.begin() + idx + gridSize);
                idx += gridSize;
                foundN = true;
                break;
            }
        }
        if (!foundN) return false;
        pages.push_back(std::move(c));
    }
    return true;
}

// ============================================================================
// Validation (public)
// ============================================================================
bool validateGrid(const Code& c) {
    return sudokuCompleteValid(c.grid, c.n);
}

// ============================================================================
// ASCII preview (single page)
// ============================================================================
std::string asciiPreview(const Code& c) {
    int n = c.n;
    int bs = boxSize(n);
    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (int r = 0; r < n; ++r) {
        if (r % bs == 0 && r != 0) {
            for (int c2 = 0; c2 < n; ++c2) {
                if (c2 % bs == 0) os << "+";
                os << "---";
            }
            os << "+\n";
        }
        for (int col = 0; col < n; ++col) {
            if (col % bs == 0) os << "|";
            uint8_t v = c.grid[r * n + col];
            if (v == 0) os << "  ";
            else os << std::setw(2) << static_cast<int>(v);
        }
        os << "|\n";
    }
    for (int c2 = 0; c2 < n; ++c2) {
        if (c2 % bs == 0) os << "+";
        os << "---";
    }
    os << "+\n";
    os << std::dec;
    return os.str();
}

} // namespace sc

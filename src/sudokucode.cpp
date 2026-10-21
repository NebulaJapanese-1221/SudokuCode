#include "sudokucode.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>

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

    // Berlekamp-Massey
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
    // Chien search: roots of lam at alpha^(i) => error at position N-1-i
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

    // Forney: compute error evaluator omega = synd * lam mod x^nsym
    std::vector<uint8_t> omega(nsym, 0);
    for (size_t i = 0; i < s.size(); ++i)
        for (size_t j = 0; j < lam.size() && i + j < omega.size(); ++j)
            omega[i + j] ^= gf::mul(s[i], lam[j]);
    // lambda'(x): formal derivative (odd-powered terms only in char 2)
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

    // verify
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
// Encode
// ============================================================================
std::vector<uint8_t> encode(const std::vector<uint8_t>& data,
                            const Params& p, Code& out) {
    gf::init();
    out.n = p.n;
    int n = p.n;
    int totalCells = n * n;

    // payload = data || crc32
    std::vector<uint8_t> payload = data;
    uint32_t c = crc32(data.data(), data.size());
    uint8_t cb[4];
    cb[0] = (c >> 24) & 0xFF; cb[1] = (c >> 16) & 0xFF;
    cb[2] = (c >> 8) & 0xFF; cb[3] = c & 0xFF;
    payload.insert(payload.end(), cb, cb + 4);

    // optional encrypt
    if (p.encrypt) payload = xorCrypt(payload, p.key);

    // RS parity count
    int nsym = 0;
    switch (p.eccLevel) {
        case 0: nsym = 0; break;
        case 1: nsym = 4; break;
        case 2: nsym = 8; break;
        case 3: nsym = 16; break;
        default: nsym = 8; break;
    }
    int k = static_cast<int>(payload.size());
    if (k + nsym > totalCells) {
        throw std::runtime_error("payload too large for grid with ECC");
    }

    // RS systematic encode
    std::vector<uint8_t> codeword;
    if (nsym == 0) {
        codeword = payload;
    } else {
        // gen polynomial
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
        // remainder of payload * x^nsym divided by gen
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

    // place codeword in grid row-major; must be sudoku-conflict-free
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

    out.grid = g;
    out.mask = fixed;
    out.length = static_cast<uint32_t>(k);
    out.crc = c;
    return codeword;
}

// ============================================================================
// Decode
// ============================================================================
std::vector<uint8_t> decode(const Code& code, const Params& p) {
    gf::init();
    int n = code.n;
    int totalCells = n * n;
    int nsym = 0;
    switch (p.eccLevel) {
        case 0: nsym = 0; break;
        case 1: nsym = 4; break;
        case 2: nsym = 8; break;
        case 3: nsym = 16; break;
        default: nsym = 8; break;
    }
    int k = static_cast<int>(code.length);
    if (k + nsym > totalCells) throw std::runtime_error("length exceeds grid");
    if (static_cast<int>(code.grid.size()) != totalCells)
        throw std::runtime_error("bad grid size");

    if (!sudokuCompleteValid(code.grid, n))
        throw std::runtime_error("invalid sudoku grid");

    std::vector<uint8_t> r(code.grid.begin(),
                           code.grid.begin() + k + nsym);

    std::vector<uint8_t> payload = rsDecode(r, nsym, k);
    if (payload.empty())
        throw std::runtime_error("Reed-Solomon decode failed (too many errors)");

    // verify CRC
    uint32_t c = 0;
    if (payload.size() >= 4) {
        c = (static_cast<uint32_t>(payload[payload.size()-4]) << 24) |
            (static_cast<uint32_t>(payload[payload.size()-3]) << 16) |
            (static_cast<uint32_t>(payload[payload.size()-2]) << 8) |
            (static_cast<uint32_t>(payload[payload.size()-1]));
    }
    std::vector<uint8_t> data(payload.begin(), payload.end() - 4);
    uint32_t calc = crc32(data.data(), data.size());
    if (calc != c)
        throw std::runtime_error("CRC32 mismatch; data corrupted");

    if (p.encrypt) data = xorCrypt(data, p.key);
    return data;
}

// ============================================================================
// SC1 file format
// ============================================================================
bool writeCodeFile(const std::string& path, const Code& c) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    auto h = [](uint8_t v) {
        const char* hex = "0123456789ABCDEF";
        std::string s; s += hex[(v >> 4) & 0xF]; s += hex[v & 0xF]; return s;
    };
    ofs << "SC1\n";
    ofs << h(static_cast<uint8_t>(c.n)) << "\n";
    ofs << h(static_cast<uint8_t>(c.length >> 8)) << h(static_cast<uint8_t>(c.length & 0xFF)) << "\n";
    ofs << h(static_cast<uint8_t>(c.crc >> 24)) << h(static_cast<uint8_t>((c.crc >> 16) & 0xFF))
        << h(static_cast<uint8_t>((c.crc >> 8) & 0xFF)) << h(static_cast<uint8_t>(c.crc & 0xFF)) << "\n";
    int n = c.n;
    for (int r = 0; r < n; ++r) {
        for (int c2 = 0; c2 < n; ++c2) ofs << h(c.grid[r * n + c2]);
        ofs << "\n";
    }
    for (int r = 0; r < n; ++r) {
        for (int c2 = 0; c2 < n; ++c2) ofs << (c.mask[r * n + c2] ? "01" : "00");
        ofs << "\n";
    }
    return ofs.good();
}

bool readCodeFile(const std::string& path, Code& c) {
    std::ifstream ifs(path);
    if (!ifs) return false;
    std::string magic;
    if (!(ifs >> magic) || magic != "SC1") return false;
    auto readByte = [&](uint8_t& v) -> bool {
        std::string tok;
        if (!(ifs >> tok)) return false;
        if (tok.size() != 2) return false;
        unsigned long val;
        try { val = std::stoul(tok, nullptr, 16); } catch (...) { return false; }
        v = static_cast<uint8_t>(val);
        return true;
    };
    uint8_t nv;
    if (!readByte(nv)) return false;
    c.n = nv;
    int n = c.n;
    c.grid.assign(n * n, 0);
    c.mask.assign(n * n, 0);
    uint8_t hi, lo;
    if (!readByte(hi) || !readByte(lo)) return false;
    c.length = (static_cast<uint32_t>(hi) << 8) | lo;
    uint8_t b0,b1,b2,b3;
    if (!readByte(b0)||!readByte(b1)||!readByte(b2)||!readByte(b3)) return false;
    c.crc = ((uint32_t)b0<<24)|((uint32_t)b1<<16)|((uint32_t)b2<<8)|b3;
    for (int i = 0; i < n * n; ++i) {
        uint8_t v;
        if (!readByte(v)) return false;
        c.grid[i] = v;
    }
    for (int i = 0; i < n * n; ++i) {
        std::string tok;
        if (!(ifs >> tok)) return false;
        c.mask[i] = (tok == "01");
    }
    return true;
}

// ============================================================================
// BMP (24-bit, no external deps)
// ============================================================================
#pragma pack(push,1)
struct BMPFileHeader { uint16_t type; uint32_t size; uint16_t reserved1; uint16_t reserved2; uint32_t offBits; };
struct BMPInfoHeader { uint32_t size; int32_t width; int32_t height; uint16_t planes; uint16_t bitCount; uint32_t compression; uint32_t sizeImage; int32_t xPels; int32_t yPels; uint32_t clrUsed; uint32_t clrImportant; };
#pragma pack(pop)

static int bmpRowStride(int w) { return (w * 3 + 3) & ~3; }

bool writeBMP(const std::string& path, const Code& c, int cellSize) {
    int n = c.n;
    int cell = cellSize > 0 ? cellSize : 20;
    int border = 2;
    int cw = cell + border;
    int w = n * cw + border;
    int h = n * cw + border;
    int stride = bmpRowStride(w);
    std::vector<uint8_t> img(stride * h, 255); // white background

    for (int r = 0; r < n; ++r) {
        for (int col = 0; col < n; ++col) {
            uint8_t v = c.grid[r * n + col];
            uint8_t fill = v;
            int x0 = border + col * cw;
            int y0 = border + r * cw;
            for (int dy = 0; dy < cell; ++dy) {
                uint8_t* row = &img[(h - 1 - (y0 + dy)) * stride + x0 * 3];
                for (int dx = 0; dx < cell; ++dx) {
                    row[dx*3 + 0] = fill;
                    row[dx*3 + 1] = fill;
                    row[dx*3 + 2] = fill;
                }
            }
        }
    }
    // draw grid lines (black)
    for (int r = 0; r <= n; ++r) {
        int y = border + r * cw;
        for (int x = 0; x < w; ++x) {
            uint8_t* row = &img[(h - 1 - y) * stride + x * 3];
            row[0]=row[1]=row[2]=0;
        }
    }
    for (int col = 0; col <= n; ++col) {
        int x = border + col * cw;
        for (int y = 0; y < h; ++y) {
            uint8_t* row = &img[(h - 1 - y) * stride + x * 3];
            row[0]=row[1]=row[2]=0;
        }
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

bool readBMP(const std::string& path, Code& c, int cellSize) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    BMPFileHeader fh; BMPInfoHeader ih;
    ifs.read(reinterpret_cast<char*>(&fh), sizeof(fh));
    ifs.read(reinterpret_cast<char*>(&ih), sizeof(ih));
    if (fh.type != 0x4D42 || ih.bitCount != 24) return false;
    int w = ih.width, h = ih.height;
    int stride = bmpRowStride(w);
    int cell = cellSize > 0 ? cellSize : 20;
    int border = 2, cw = cell + border;
    if ((w - border) % cw != 0) return false;
    int n = (w - border) / cw;
    if (n != 4 && n != 9 && n != 16) return false;
    c.n = n;
    c.grid.assign(n * n, 0);
    c.mask.assign(n * n, 1);
    for (int r = 0; r < n; ++r) {
        for (int col = 0; col < n; ++col) {
            int cx = border + col * cw + cell / 2;
            int cy = border + r * cw + cell / 2;
            if (cx < 0 || cx >= w || cy < 0 || cy >= h) return false;
            uint8_t px[3];
            ifs.seekg(fh.offBits + static_cast<std::streamoff>((h - 1 - cy) * stride + cx * 3));
            ifs.read(reinterpret_cast<char*>(px), 3);
            c.grid[r * n + col] = px[2]; // R channel
        }
    }
    c.length = static_cast<uint32_t>(n * n);
    c.crc = 0;
    return true;
}

// ============================================================================
// Validation (public)
// ============================================================================
bool validateGrid(const Code& c) {
    return sudokuCompleteValid(c.grid, c.n);
}

// ============================================================================
// ASCII preview
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

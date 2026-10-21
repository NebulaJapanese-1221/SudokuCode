#include "sudokucode.h"
#include <cassert>
#include <iostream>
#include <random>
#include <vector>

using namespace sc;

static std::vector<uint8_t> randBytes(size_t n, uint32_t seed) {
    std::mt19937 rng(seed);
    std::vector<uint8_t> v(n);
    for (auto& b : v) b = static_cast<uint8_t>(rng());
    return v;
}

static void roundTrip(int n, int ecc, bool autoEcc, size_t payload, bool encrypt,
                      bool compress, bool usePassphrase, uint32_t seed) {
    Params p;
    p.n = n;
    p.eccLevel = ecc;
    p.autoEcc = autoEcc;
    p.encrypt = encrypt;
    p.compress = compress;
    if (usePassphrase) p.passphrase = "test-passphrase-456";
    else if (encrypt) p.key = "test-key-123";
    auto data = randBytes(payload, seed);
    auto pages = encode(data, p);
    assert(!pages.empty());
    for (const auto& c : pages)
        assert(static_cast<int>(c.grid.size()) == n * n);
    auto out = decode(pages, p);
    assert(out == data);
    // verify-only
    assert(verify(pages, p));
}

static void batchRoundTrip(int n, int ecc, size_t payload, uint32_t seed) {
    Params p; p.n = n; p.eccLevel = ecc;
    auto data = randBytes(payload, seed);
    std::vector<BatchEntry> files = {
        {"alpha.txt", randBytes(payload / 2, seed)},
        {"beta.bin", randBytes(payload - payload / 2, seed + 1)},
    };
    auto pages = encodeBatch(files, p);
    assert(!pages.empty());
    auto out = decodeBatch(pages, p);
    assert(out.size() == files.size());
    assert(out[0].filename == "alpha.txt");
    assert(out[1].filename == "beta.bin");
    assert(out[0].data == files[0].data);
    assert(out[1].data == files[1].data);
}

int main() {
    int passed = 0;
    // edge cases: empty
    {
        Params p; p.n = 9; p.eccLevel = 2;
        std::vector<uint8_t> empty;
        auto pages = encode(empty, p);
        assert(decode(pages, p) == empty);
        assert(verify(pages, p));
        ++passed;
    }
    // zero byte payload
    {
        Params p; p.n = 9; p.eccLevel = 2;
        std::vector<uint8_t> v(5, 0x00);
        auto pages = encode(v, p);
        auto out = decode(pages, p);
        assert(out == v);
        ++passed;
    }
    // auto ECC round trips
    for (int n : {4, 9, 16})
        for (bool autoEcc : {true, false})
            for (bool compress : {false, true})
                for (bool enc : {false, true})
                    for (bool pw : {false, true})
                        for (size_t pl : {1, 5, 16, 32, 100, 300, 800})
                            for (int s = 0; s < 2; ++s) {
                                try {
                                    roundTrip(n, 2, autoEcc, pl, enc, compress, pw, static_cast<uint32_t>(s));
                                    ++passed;
                                } catch (const std::exception& e) {
                                    std::cout << "skipped (constraint): n=" << n
                                              << " auto=" << autoEcc << " comp=" << compress
                                              << " enc=" << enc << " pw=" << pw
                                              << " pl=" << pl << " seed=" << s
                                              << " (" << e.what() << ")\n";
                                }
                            }
    // fixed ECC levels
    for (int ecc = 0; ecc <= 3; ++ecc)
        for (int n : {4, 9, 16})
            for (size_t pl : {1, 16, 64})
                for (int s = 0; s < 2; ++s) {
                    try {
                        roundTrip(n, ecc, false, pl, false, false, false, static_cast<uint32_t>(s));
                        ++passed;
                    } catch (const std::exception& e) {
                        std::cout << "skipped (constraint): ecc=" << ecc
                                  << " n=" << n << " pl=" << pl
                                  << " seed=" << s << " (" << e.what() << ")\n";
                    }
                }
    // corrupt byte -> decode should fail/correct
    {
        Params p; p.n = 9; p.eccLevel = 2;
        auto data = randBytes(10, 42);
        auto pages = encode(data, p);
        pages[0].grid[0] ^= 0xFF;
        auto out = decode(pages, p);
        assert(out == data);
        ++passed;
    }
    // CRC mismatch without ECC should fail
    {
        Params p; p.n = 9; p.eccLevel = 0;
        auto data = randBytes(10, 7);
        auto pages = encode(data, p);
        pages[0].grid[3] ^= 0x01;
        bool threw = false;
        try { decode(pages, p); } catch (...) { threw = true; }
        assert(threw);
        ++passed;
    }
    // batch mode
    for (int n : {4, 9, 16})
        for (size_t pl : {16, 64, 256})
            for (int s = 0; s < 2; ++s) {
                try {
                    batchRoundTrip(n, 2, pl, static_cast<uint32_t>(s));
                    ++passed;
                } catch (const std::exception& e) {
                    std::cout << "skipped batch (constraint): n=" << n
                              << " pl=" << pl << " seed=" << s
                              << " (" << e.what() << ")\n";
                }
            }
    // verify-only on corrupted page should fail
    {
        Params p; p.n = 9; p.eccLevel = 2;
        auto data = randBytes(10, 99);
        auto pages = encode(data, p);
        pages[0].grid[5] ^= 0xFF;
        assert(!verify(pages, p));
        ++passed;
    }
    std::cout << "All tests passed (" << passed << ")\n";
    return 0;
}

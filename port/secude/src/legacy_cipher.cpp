// DES/3DES (FIPS 46-3), IDEA, RC2 (RFC 2268) and RC4.
#include "legacy.hpp"

#include <array>

namespace compat {
namespace legacy {

namespace {

// ---- DES ----------------------------------------------------------------------------

const uint8_t kIP[64] = {58, 50, 42, 34, 26, 18, 10, 2, 60, 52, 44, 36, 28, 20, 12, 4,
                         62, 54, 46, 38, 30, 22, 14, 6, 64, 56, 48, 40, 32, 24, 16, 8,
                         57, 49, 41, 33, 25, 17, 9,  1, 59, 51, 43, 35, 27, 19, 11, 3,
                         61, 53, 45, 37, 29, 21, 13, 5, 63, 55, 47, 39, 31, 23, 15, 7};
const uint8_t kFP[64] = {40, 8, 48, 16, 56, 24, 64, 32, 39, 7, 47, 15, 55, 23, 63, 31,
                         38, 6, 46, 14, 54, 22, 62, 30, 37, 5, 45, 13, 53, 21, 61, 29,
                         36, 4, 44, 12, 52, 20, 60, 28, 35, 3, 43, 11, 51, 19, 59, 27,
                         34, 2, 42, 10, 50, 18, 58, 26, 33, 1, 41, 9,  49, 17, 57, 25};
const uint8_t kE[48] = {32, 1,  2,  3,  4,  5,  4,  5,  6,  7,  8,  9,  8,  9,  10, 11,
                        12, 13, 12, 13, 14, 15, 16, 17, 16, 17, 18, 19, 20, 21, 20, 21,
                        22, 23, 24, 25, 24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32, 1};
const uint8_t kP[32] = {16, 7, 20, 21, 29, 12, 28, 17, 1,  15, 23, 26, 5,  18, 31, 10,
                        2,  8, 24, 14, 32, 27, 3,  9,  19, 13, 30, 6,  22, 11, 4,  25};
const uint8_t kPC1[56] = {57, 49, 41, 33, 25, 17, 9,  1,  58, 50, 42, 34, 26, 18, 10, 2,  59, 51, 43,
                          35, 27, 19, 11, 3,  60, 52, 44, 36, 63, 55, 47, 39, 31, 23, 15, 7,  62, 54,
                          46, 38, 30, 22, 14, 6,  61, 53, 45, 37, 29, 21, 13, 5,  28, 20, 12, 4};
const uint8_t kPC2[48] = {14, 17, 11, 24, 1,  5,  3,  28, 15, 6,  21, 10, 23, 19, 12, 4,
                          26, 8,  16, 7,  27, 20, 13, 2,  41, 52, 31, 37, 47, 55, 30, 40,
                          51, 45, 33, 48, 44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32};
const uint8_t kShifts[16] = {1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1};
const uint8_t kS[8][64] = {
    {14, 4,  13, 1, 2,  15, 11, 8,  3,  10, 6,  12, 5,  9,  0, 7,  0, 15, 7,  4,  14, 2,
     13, 1,  10, 6, 12, 11, 9,  5,  3,  8,  4,  1,  14, 8,  13, 6, 2, 11, 15, 12, 9,  7,
     3,  10, 5,  0, 15, 12, 8,  2,  4,  9,  1,  7,  5,  11, 3, 14, 10, 0, 6,  13},
    {15, 1,  8,  14, 6,  11, 3,  4,  9,  7, 2,  13, 12, 0, 5,  10, 3,  13, 4,  7,  15, 2,
     8,  14, 12, 0,  1,  10, 6,  9,  11, 5, 0,  14, 7,  11, 10, 4,  13, 1,  5,  8,  12, 6,
     9,  3,  2,  15, 13, 8,  10, 1,  3,  15, 4,  2,  11, 6,  7,  12, 0, 5,  14, 9},
    {10, 0,  9,  14, 6,  3,  15, 5,  1, 13, 12, 7,  11, 4,  2,  8, 13, 7, 0,  9,  3,  4,
     6,  10, 2,  8,  5,  14, 12, 11, 15, 1, 13, 6,  4,  9,  8, 15, 3,  0,  11, 1,  2,  12,
     5,  10, 14, 7,  1,  10, 13, 0,  6,  9,  8,  7, 4,  15, 14, 3,  11, 5,  2,  12},
    {7,  13, 14, 3,  0,  6,  9,  10, 1, 2, 8,  5,  11, 12, 4, 15, 13, 8,  11, 5,  6,  15,
     0,  3,  4,  7,  2,  12, 1,  10, 14, 9, 10, 6,  9,  0,  12, 11, 7,  13, 15, 1,  3,  14,
     5,  2,  8,  4,  3,  15, 0,  6,  10, 1, 13, 8,  9,  4,  5,  11, 12, 7,  2,  14},
    {2,  12, 4, 1,  7,  10, 11, 6,  8, 5,  3,  15, 13, 0,  14, 9,  14, 11, 2,  12, 4,  7,
     13, 1,  5, 0,  15, 10, 3,  9,  8, 6,  4,  2,  1,  11, 10, 13, 7,  8,  15, 9,  12, 5,
     6,  3,  0, 14, 11, 8,  12, 7,  1, 14, 2,  13, 6,  15, 0,  9,  10, 4,  5,  3},
    {12, 1,  10, 15, 9,  2,  6,  8,  0,  13, 3,  4,  14, 7,  5,  11, 10, 15, 4, 2,  7,  12,
     9,  5,  6,  1,  13, 14, 0,  11, 3,  8,  9,  14, 15, 5,  2,  8,  12, 3,  7, 0,  4,  10,
     1,  13, 11, 6,  4,  3,  2,  12, 9,  5,  15, 10, 11, 14, 1,  7,  6,  0,  8, 13},
    {4, 11, 2,  14, 15, 0, 8,  13, 3,  12, 9,  7,  5,  10, 6, 1,  13, 0,  11, 7,  4,  9,
     1, 10, 14, 3,  5,  12, 2, 15, 8,  6,  1,  4,  11, 13, 12, 3, 7,  14, 10, 15, 6,  8,
     0, 5,  9,  2,  6,  11, 13, 8, 1,  4,  10, 7,  9,  5,  0,  15, 14, 2, 3,  12},
    {13, 2,  8, 4,  6,  15, 11, 1, 10, 9,  3,  14, 5,  0,  12, 7,  1, 15, 13, 8,  10, 3,
     7,  4,  12, 5, 6,  11, 0,  14, 9,  2,  7,  11, 4,  1,  9,  12, 14, 2, 0,  6,  10, 13,
     15, 3,  5,  8, 2,  1,  14, 7,  4,  10, 8,  13, 15, 12, 9,  0,  3,  5, 6,  11},
};

uint64_t permute(uint64_t in, const uint8_t *table, int n, int inbits)
{
    uint64_t out = 0;
    for (int i = 0; i < n; ++i)
        out = (out << 1) | ((in >> (inbits - table[i])) & 1);
    return out;
}

struct SpTables {
    uint32_t sp[8][64];
    SpTables()
    {
        for (int box = 0; box < 8; ++box)
            for (int six = 0; six < 64; ++six) {
                int row = ((six >> 4) & 2) | (six & 1);
                int col = (six >> 1) & 0xf;
                uint64_t s = uint64_t(kS[box][row * 16 + col]) << (28 - 4 * box);
                sp[box][six] = uint32_t(permute(s, kP, 32, 32));
            }
    }
};

const SpTables &sp_tables()
{
    static const SpTables t;
    return t;
}

uint64_t load64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v = (v << 8) | p[i];
    return v;
}

void store64(uint8_t *p, uint64_t v)
{
    for (int i = 7; i >= 0; --i) {
        p[i] = uint8_t(v);
        v >>= 8;
    }
}

class DesKey {
public:
    explicit DesKey(const uint8_t key[8])
    {
        uint64_t cd = permute(load64(key), kPC1, 56, 64);
        uint32_t c = uint32_t(cd >> 28) & 0x0fffffff, d = uint32_t(cd) & 0x0fffffff;
        for (int i = 0; i < 16; ++i) {
            for (int s = 0; s < kShifts[i]; ++s) {
                c = ((c << 1) | (c >> 27)) & 0x0fffffff;
                d = ((d << 1) | (d >> 27)) & 0x0fffffff;
            }
            sub_[i] = permute((uint64_t(c) << 28) | d, kPC2, 48, 56);
        }
    }

    uint64_t crypt(uint64_t block, bool decrypt) const
    {
        const SpTables &t = sp_tables();
        uint64_t ip = permute(block, kIP, 64, 64);
        uint32_t l = uint32_t(ip >> 32), r = uint32_t(ip);
        for (int i = 0; i < 16; ++i) {
            uint64_t e = permute(r, kE, 48, 32) ^ sub_[decrypt ? 15 - i : i];
            uint32_t f = 0;
            for (int box = 0; box < 8; ++box)
                f |= t.sp[box][(e >> (42 - 6 * box)) & 0x3f];
            uint32_t nl = r;
            r = l ^ f;
            l = nl;
        }
        return permute((uint64_t(r) << 32) | l, kFP, 64, 64);
    }

private:
    uint64_t sub_[16];
};

class Des : public BlockCipher {
public:
    explicit Des(const uint8_t key[8]) : k_(key) {}
    size_t block_size() const override { return 8; }
    void encrypt(const uint8_t *in, uint8_t *out) const override { store64(out, k_.crypt(load64(in), false)); }
    void decrypt(const uint8_t *in, uint8_t *out) const override { store64(out, k_.crypt(load64(in), true)); }

private:
    DesKey k_;
};

class Des3 : public BlockCipher {
public:
    Des3(const uint8_t *key, size_t keylen) : k1_(key), k2_(key + 8), k3_(keylen >= 24 ? key + 16 : key) {}
    size_t block_size() const override { return 8; }
    void encrypt(const uint8_t *in, uint8_t *out) const override
    {
        store64(out, k3_.crypt(k2_.crypt(k1_.crypt(load64(in), false), true), false));
    }
    void decrypt(const uint8_t *in, uint8_t *out) const override
    {
        store64(out, k1_.crypt(k2_.crypt(k3_.crypt(load64(in), true), false), true));
    }

private:
    DesKey k1_, k2_, k3_;
};

// ---- IDEA ---------------------------------------------------------------------------

uint16_t idea_mul(uint32_t a, uint32_t b)
{
    if (!a)
        a = 0x10000;
    if (!b)
        b = 0x10000;
    uint32_t p = uint32_t((uint64_t(a) * b) % 0x10001);
    return uint16_t(p & 0xffff);
}

uint16_t idea_inv(uint16_t x)
{
    if (x <= 1)
        return x;
    // Fermat: x^(p-2) mod p for the prime p = 65537
    uint64_t result = 1, base = x, e = 0x10001 - 2;
    while (e) {
        if (e & 1)
            result = (result * base) % 0x10001;
        base = (base * base) % 0x10001;
        e >>= 1;
    }
    return uint16_t(result);
}

class Idea : public BlockCipher {
public:
    explicit Idea(const uint8_t key[16])
    {
        uint16_t k[8];
        for (int i = 0; i < 8; ++i)
            k[i] = uint16_t((key[2 * i] << 8) | key[2 * i + 1]);
        for (int i = 0; i < 52; ++i) {
            if (i && i % 8 == 0) {
                uint16_t r[8];
                for (int j = 0; j < 8; ++j)
                    r[j] = uint16_t((k[(j + 1) % 8] << 9) | (k[(j + 2) % 8] >> 7));
                std::memcpy(k, r, sizeof k);
            }
            ek_[i] = k[i % 8];
        }
        dk_[0] = idea_inv(ek_[48]);
        dk_[1] = uint16_t(-ek_[49]);
        dk_[2] = uint16_t(-ek_[50]);
        dk_[3] = idea_inv(ek_[51]);
        for (int r = 1; r < 8; ++r) {
            int e = 48 - 6 * r;
            dk_[6 * r - 2] = ek_[e + 4];
            dk_[6 * r - 1] = ek_[e + 5];
            dk_[6 * r] = idea_inv(ek_[e]);
            dk_[6 * r + 1] = uint16_t(-ek_[e + 2]);
            dk_[6 * r + 2] = uint16_t(-ek_[e + 1]);
            dk_[6 * r + 3] = idea_inv(ek_[e + 3]);
        }
        dk_[46] = ek_[4];
        dk_[47] = ek_[5];
        dk_[48] = idea_inv(ek_[0]);
        dk_[49] = uint16_t(-ek_[1]);
        dk_[50] = uint16_t(-ek_[2]);
        dk_[51] = idea_inv(ek_[3]);
    }
    size_t block_size() const override { return 8; }
    void encrypt(const uint8_t *in, uint8_t *out) const override { crypt(ek_, in, out); }
    void decrypt(const uint8_t *in, uint8_t *out) const override { crypt(dk_, in, out); }

private:
    static void crypt(const uint16_t *k, const uint8_t *in, uint8_t *out)
    {
        uint16_t x1 = uint16_t((in[0] << 8) | in[1]), x2 = uint16_t((in[2] << 8) | in[3]);
        uint16_t x3 = uint16_t((in[4] << 8) | in[5]), x4 = uint16_t((in[6] << 8) | in[7]);
        for (int r = 0; r < 8; ++r, k += 6) {
            x1 = idea_mul(x1, k[0]);
            x2 = uint16_t(x2 + k[1]);
            x3 = uint16_t(x3 + k[2]);
            x4 = idea_mul(x4, k[3]);
            uint16_t t0 = idea_mul(uint16_t(x1 ^ x3), k[4]);
            uint16_t t1 = idea_mul(uint16_t(t0 + (x2 ^ x4)), k[5]);
            t0 = uint16_t(t0 + t1);
            x1 ^= t1;
            x4 ^= t0;
            uint16_t t = uint16_t(x2 ^ t0);
            x2 = uint16_t(x3 ^ t1);
            x3 = t;
        }
        uint16_t y1 = idea_mul(x1, k[0]), y2 = uint16_t(x3 + k[1]), y3 = uint16_t(x2 + k[2]), y4 = idea_mul(x4, k[3]);
        const uint16_t y[4] = {y1, y2, y3, y4};
        for (int i = 0; i < 4; ++i) {
            out[2 * i] = uint8_t(y[i] >> 8);
            out[2 * i + 1] = uint8_t(y[i]);
        }
    }

    uint16_t ek_[52];
    uint16_t dk_[52];
};

// ---- RC2 ----------------------------------------------------------------------------

const uint8_t kPiTable[256] = {
    0xd9, 0x78, 0xf9, 0xc4, 0x19, 0xdd, 0xb5, 0xed, 0x28, 0xe9, 0xfd, 0x79, 0x4a, 0xa0, 0xd8, 0x9d, 0xc6, 0x7e, 0x37,
    0x83, 0x2b, 0x76, 0x53, 0x8e, 0x62, 0x4c, 0x64, 0x88, 0x44, 0x8b, 0xfb, 0xa2, 0x17, 0x9a, 0x59, 0xf5, 0x87, 0xb3,
    0x4f, 0x13, 0x61, 0x45, 0x6d, 0x8d, 0x09, 0x81, 0x7d, 0x32, 0xbd, 0x8f, 0x40, 0xeb, 0x86, 0xb7, 0x7b, 0x0b, 0xf0,
    0x95, 0x21, 0x22, 0x5c, 0x6b, 0x4e, 0x82, 0x54, 0xd6, 0x65, 0x93, 0xce, 0x60, 0xb2, 0x1c, 0x73, 0x56, 0xc0, 0x14,
    0xa7, 0x8c, 0xf1, 0xdc, 0x12, 0x75, 0xca, 0x1f, 0x3b, 0xbe, 0xe4, 0xd1, 0x42, 0x3d, 0xd4, 0x30, 0xa3, 0x3c, 0xb6,
    0x26, 0x6f, 0xbf, 0x0e, 0xda, 0x46, 0x69, 0x07, 0x57, 0x27, 0xf2, 0x1d, 0x9b, 0xbc, 0x94, 0x43, 0x03, 0xf8, 0x11,
    0xc7, 0xf6, 0x90, 0xef, 0x3e, 0xe7, 0x06, 0xc3, 0xd5, 0x2f, 0xc8, 0x66, 0x1e, 0xd7, 0x08, 0xe8, 0xea, 0xde, 0x80,
    0x52, 0xee, 0xf7, 0x84, 0xaa, 0x72, 0xac, 0x35, 0x4d, 0x6a, 0x2a, 0x96, 0x1a, 0xd2, 0x71, 0x5a, 0x15, 0x49, 0x74,
    0x4b, 0x9f, 0xd0, 0x5e, 0x04, 0x18, 0xa4, 0xec, 0xc2, 0xe0, 0x41, 0x6e, 0x0f, 0x51, 0xcb, 0xcc, 0x24, 0x91, 0xaf,
    0x50, 0xa1, 0xf4, 0x70, 0x39, 0x99, 0x7c, 0x3a, 0x85, 0x23, 0xb8, 0xb4, 0x7a, 0xfc, 0x02, 0x36, 0x5b, 0x25, 0x55,
    0x97, 0x31, 0x2d, 0x5d, 0xfa, 0x98, 0xe3, 0x8a, 0x92, 0xae, 0x05, 0xdf, 0x29, 0x10, 0x67, 0x6c, 0xba, 0xc9, 0xd3,
    0x00, 0xe6, 0xcf, 0xe1, 0x9e, 0xa8, 0x2c, 0x63, 0x16, 0x01, 0x3f, 0x58, 0xe2, 0x89, 0xa9, 0x0d, 0x38, 0x34, 0x1b,
    0xab, 0x33, 0xff, 0xb0, 0xbb, 0x48, 0x0c, 0x5f, 0xb9, 0xb1, 0xcd, 0x2e, 0xc5, 0xf3, 0xdb, 0x47, 0xe5, 0xa5, 0x9c,
    0x77, 0x0a, 0xa6, 0x20, 0x68, 0xfe, 0x7f, 0xc1, 0xad,
};

class Rc2 : public BlockCipher {
public:
    Rc2(const uint8_t *key, size_t keylen, unsigned bits)
    {
        uint8_t l[128] = {0};
        size_t t = std::min<size_t>(keylen, 128);
        std::memcpy(l, key, t);
        for (size_t i = t; i < 128; ++i)
            l[i] = kPiTable[uint8_t(l[i - 1] + l[i - t])];
        if (bits == 0 || bits > 1024)
            bits = 1024;
        size_t t8 = (bits + 7) / 8;
        uint8_t tm = uint8_t(0xff >> (8 * t8 - bits));
        l[128 - t8] = kPiTable[l[128 - t8] & tm];
        for (size_t i = 128 - t8; i-- > 0;)
            l[i] = kPiTable[l[i + 1] ^ l[i + t8]];
        for (int i = 0; i < 64; ++i)
            k_[i] = uint16_t(l[2 * i] | (l[2 * i + 1] << 8));
    }
    size_t block_size() const override { return 8; }

    void encrypt(const uint8_t *in, uint8_t *out) const override
    {
        uint16_t r[4];
        load(in, r);
        int j = 0;
        for (int round = 0; round < 16; ++round) {
            for (int i = 0; i < 4; ++i) {
                r[i] = uint16_t(r[i] + k_[j++] + (r[(i + 3) % 4] & r[(i + 2) % 4]) + (~r[(i + 3) % 4] & r[(i + 1) % 4]));
                r[i] = rol16(r[i], kRot[i]);
            }
            if (round == 4 || round == 10)
                for (int i = 0; i < 4; ++i)
                    r[i] = uint16_t(r[i] + k_[r[(i + 3) % 4] & 63]);
        }
        store(out, r);
    }

    void decrypt(const uint8_t *in, uint8_t *out) const override
    {
        uint16_t r[4];
        load(in, r);
        int j = 63;
        for (int round = 15; round >= 0; --round) {
            for (int i = 3; i >= 0; --i) {
                r[i] = ror16(r[i], kRot[i]);
                r[i] = uint16_t(r[i] - k_[j--] - (r[(i + 3) % 4] & r[(i + 2) % 4]) - (~r[(i + 3) % 4] & r[(i + 1) % 4]));
            }
            if (round == 5 || round == 11)
                for (int i = 3; i >= 0; --i)
                    r[i] = uint16_t(r[i] - k_[r[(i + 3) % 4] & 63]);
        }
        store(out, r);
    }

private:
    static constexpr int kRot[4] = {1, 2, 3, 5};
    static uint16_t rol16(uint16_t x, int n) { return uint16_t((x << n) | (x >> (16 - n))); }
    static uint16_t ror16(uint16_t x, int n) { return uint16_t((x >> n) | (x << (16 - n))); }
    static void load(const uint8_t *in, uint16_t r[4])
    {
        for (int i = 0; i < 4; ++i)
            r[i] = uint16_t(in[2 * i] | (in[2 * i + 1] << 8));
    }
    static void store(uint8_t *out, const uint16_t r[4])
    {
        for (int i = 0; i < 4; ++i) {
            out[2 * i] = uint8_t(r[i]);
            out[2 * i + 1] = uint8_t(r[i] >> 8);
        }
    }
    uint16_t k_[64];
};

} // namespace

std::unique_ptr<BlockCipher> make_des(const uint8_t key[8]) { return std::make_unique<Des>(key); }

std::unique_ptr<BlockCipher> make_des3(const uint8_t *key, size_t keylen)
{
    return std::make_unique<Des3>(key, keylen);
}

std::unique_ptr<BlockCipher> make_idea(const uint8_t key[16]) { return std::make_unique<Idea>(key); }

std::unique_ptr<BlockCipher> make_rc2(const uint8_t *key, size_t keylen, unsigned effective_bits)
{
    return std::make_unique<Rc2>(key, keylen, effective_bits);
}

void rc4(const uint8_t *key, size_t keylen, const uint8_t *in, uint8_t *out, size_t n)
{
    uint8_t s[256];
    for (int i = 0; i < 256; ++i)
        s[i] = uint8_t(i);
    uint8_t j = 0;
    for (int i = 0; i < 256; ++i) {
        j = uint8_t(j + s[i] + key[size_t(i) % keylen]);
        std::swap(s[i], s[j]);
    }
    uint8_t x = 0, y = 0;
    for (size_t k = 0; k < n; ++k) {
        x = uint8_t(x + 1);
        y = uint8_t(y + s[x]);
        std::swap(s[x], s[y]);
        out[k] = in[k] ^ s[uint8_t(s[x] + s[y])];
    }
}

} // namespace legacy
} // namespace compat

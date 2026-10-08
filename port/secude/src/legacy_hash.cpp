// MD2 (RFC 1319), MD4 (RFC 1320), MD5 (RFC 1321), SHA-0/SHA-1 (FIPS 180/180-1)
// and RIPEMD-160, operating on the SECUDE context structures.
#include "legacy.hpp"

#include <algorithm>

namespace compat {
namespace legacy {

namespace {

inline uint32_t rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

inline uint32_t load_le(const uint8_t *p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

inline uint32_t load_be(const uint8_t *p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

inline void store_le(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        p[i] = uint8_t(v >> (8 * i));
}

inline void store_be(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        p[i] = uint8_t(v >> (24 - 8 * i));
}

// ---- MD2 ----------------------------------------------------------------------

const uint8_t kPiSubst[256] = {
    41,  46,  67,  201, 162, 216, 124, 1,   61,  54,  84,  161, 236, 240, 6,   19,  98,  167, 5,   243, 192, 199,
    115, 140, 152, 147, 43,  217, 188, 76,  130, 202, 30,  155, 87,  60,  253, 212, 224, 22,  103, 66,  111, 24,
    138, 23,  229, 18,  190, 78,  196, 214, 218, 158, 222, 73,  160, 251, 245, 142, 187, 47,  238, 122, 169, 104,
    121, 145, 21,  178, 7,   63,  148, 194, 16,  137, 11,  34,  95,  33,  128, 127, 93,  154, 90,  144, 50,  39,
    53,  62,  204, 231, 191, 247, 151, 3,   255, 25,  48,  179, 72,  165, 181, 209, 215, 94,  146, 42,  172, 86,
    170, 198, 79,  184, 56,  210, 150, 164, 125, 182, 118, 252, 107, 226, 156, 116, 4,   241, 69,  157, 112, 89,
    100, 113, 135, 32,  134, 91,  207, 101, 230, 45,  168, 2,   27,  96,  37,  173, 174, 176, 185, 246, 28,  70,
    97,  105, 52,  64,  126, 15,  85,  71,  163, 35,  221, 81,  175, 58,  195, 92,  249, 206, 186, 197, 234, 38,
    44,  83,  13,  110, 133, 40,  132, 9,   211, 223, 205, 244, 65,  129, 77,  82,  106, 220, 55,  200, 108, 193,
    171, 250, 36,  225, 123, 8,   12,  189, 177, 74,  120, 136, 149, 139, 227, 99,  232, 109, 233, 203, 213, 254,
    59,  0,   29,  57,  242, 239, 183, 14,  102, 88,  208, 228, 166, 119, 114, 248, 235, 117, 75,  10,  49,  68,
    80,  180, 143, 237, 31,  26,  219, 153, 141, 51,  159, 17,  131, 20,
};

void md2_transform(SEC_MD2_CTX *c, const uint8_t *block)
{
    uint8_t x[48];
    for (int i = 0; i < 16; ++i) {
        x[i] = c->state[i];
        x[i + 16] = block[i];
        x[i + 32] = uint8_t(c->state[i] ^ block[i]);
    }
    unsigned t = 0;
    for (unsigned round = 0; round < 18; ++round) {
        for (int k = 0; k < 48; ++k)
            t = x[k] ^= kPiSubst[t];
        t = (t + round) & 0xff;
    }
    for (int i = 0; i < 16; ++i)
        c->state[i] = x[i];
    t = c->checksum[15];
    for (int i = 0; i < 16; ++i)
        t = c->checksum[i] ^= kPiSubst[block[i] ^ t];
}

// ---- MD4 / MD5 ------------------------------------------------------------------

#define MD_F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define MD4_G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define MD5_G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define MD_H(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_I(x, y, z) ((y) ^ ((x) | ~(z)))

void md4_transform(uint32_t state[4], const uint8_t *block)
{
    uint32_t x[16];
    for (int i = 0; i < 16; ++i)
        x[i] = load_le(block + 4 * i);
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    static const int r1[4] = {3, 7, 11, 19}, r2[4] = {3, 5, 9, 13}, r3[4] = {3, 9, 11, 15};
    for (int i = 0; i < 16; ++i) {
        uint32_t t = rol(a + MD_F(b, c, d) + x[i], r1[i % 4]);
        a = d; d = c; c = b; b = t;
    }
    static const int o2[16] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
    for (int i = 0; i < 16; ++i) {
        uint32_t t = rol(a + MD4_G(b, c, d) + x[o2[i]] + 0x5a827999u, r2[i % 4]);
        a = d; d = c; c = b; b = t;
    }
    static const int o3[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
    for (int i = 0; i < 16; ++i) {
        uint32_t t = rol(a + MD_H(b, c, d) + x[o3[i]] + 0x6ed9eba1u, r3[i % 4]);
        a = d; d = c; c = b; b = t;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

void md5_transform(uint32_t state[4], const uint8_t *block)
{
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
    };
    static const int S[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                              5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                              6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    uint32_t x[16];
    for (int i = 0; i < 16; ++i)
        x[i] = load_le(block + 4 * i);
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    for (int i = 0; i < 64; ++i) {
        uint32_t f;
        int g;
        if (i < 16) {
            f = MD_F(b, c, d);
            g = i;
        } else if (i < 32) {
            f = MD5_G(b, c, d);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = MD_H(b, c, d);
            g = (3 * i + 5) % 16;
        } else {
            f = MD5_I(b, c, d);
            g = (7 * i) % 16;
        }
        uint32_t t = d;
        d = c;
        c = b;
        b = b + rol(a + f + K[i] + x[g], S[i]);
        a = t;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

// Shared Merkle-Damgard bookkeeping of the RFC 1320/1321 contexts.
template <class Ctx, void (*Transform)(uint32_t *, const uint8_t *)>
void md_update(Ctx *c, const uint8_t *p, size_t n)
{
    if (n == 0)
        return;
    size_t index = (c->count[0] >> 3) & 0x3f;
    uint64_t bits = (uint64_t(c->count[1]) << 32 | c->count[0]) + (uint64_t(n) << 3);
    c->count[0] = uint32_t(bits);
    c->count[1] = uint32_t(bits >> 32);
    size_t part = 64 - index;
    size_t i = 0;
    if (n >= part) {
        std::memcpy(c->buffer + index, p, part);
        Transform(c->state, c->buffer);
        for (i = part; i + 63 < n; i += 64)
            Transform(c->state, p + i);
        index = 0;
    }
    std::memcpy(c->buffer + index, p + i, n - i);
}

template <class Ctx, void (*Transform)(uint32_t *, const uint8_t *)>
void md_final(uint8_t digest[16], Ctx *c)
{
    uint8_t bits[8];
    store_le(bits, c->count[0]);
    store_le(bits + 4, c->count[1]);
    size_t index = (c->count[0] >> 3) & 0x3f;
    size_t padlen = index < 56 ? 56 - index : 120 - index;
    static const uint8_t pad[64] = {0x80};
    md_update<Ctx, Transform>(c, pad, padlen);
    md_update<Ctx, Transform>(c, bits, 8);
    for (int i = 0; i < 4; ++i)
        store_le(digest + 4 * i, c->state[i]);
    std::memset(c, 0, sizeof *c);
}

template <class Ctx> void md_init(Ctx *c)
{
    std::memset(c, 0, sizeof *c);
    c->state[0] = 0x67452301;
    c->state[1] = 0xefcdab89;
    c->state[2] = 0x98badcfe;
    c->state[3] = 0x10325476;
}

// ---- SHA-0 / SHA-1 on SHS_INFO (countLo/countHi count bits) ---------------------

void shs_transform(SHS_INFO *s, const uint8_t *block, bool sha1)
{
    uint32_t w[80];
    for (int i = 0; i < 16; ++i)
        w[i] = load_be(block + 4 * i);
    for (int i = 16; i < 80; ++i) {
        uint32_t t = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
        w[i] = sha1 ? rol(t, 1) : t;
    }
    uint32_t a = s->digest[0], b = s->digest[1], c = s->digest[2], d = s->digest[3], e = s->digest[4];
    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5a827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdc;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6;
        }
        uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = t;
    }
    s->digest[0] += a; s->digest[1] += b; s->digest[2] += c; s->digest[3] += d; s->digest[4] += e;
}

void shs_init(SHS_INFO *s)
{
    std::memset(s, 0, sizeof *s);
    s->digest[0] = 0x67452301;
    s->digest[1] = 0xefcdab89;
    s->digest[2] = 0x98badcfe;
    s->digest[3] = 0x10325476;
    s->digest[4] = 0xc3d2e1f0;
}

void shs_update(SHS_INFO *s, const uint8_t *p, size_t n, bool sha1)
{
    uint8_t *buf = reinterpret_cast<uint8_t *>(s->data);
    size_t index = (s->countLo >> 3) & 0x3f;
    uint64_t bits = (uint64_t(s->countHi) << 32 | s->countLo) + (uint64_t(n) << 3);
    s->countLo = uint32_t(bits);
    s->countHi = uint32_t(bits >> 32);
    while (n) {
        size_t take = std::min(n, size_t(64) - index);
        std::memcpy(buf + index, p, take);
        index += take;
        p += take;
        n -= take;
        if (index == 64) {
            uint8_t block[64];
            std::memcpy(block, buf, 64);
            shs_transform(s, block, sha1);
            index = 0;
        }
    }
}

void shs_final(SHS_INFO *s, bool sha1)
{
    uint8_t len[8];
    store_be(len, s->countHi);
    store_be(len + 4, s->countLo);
    size_t index = (s->countLo >> 3) & 0x3f;
    size_t padlen = index < 56 ? 56 - index : 120 - index;
    static const uint8_t pad[64] = {0x80};
    shs_update(s, pad, padlen, sha1);
    shs_update(s, len, 8, sha1);
}

// ---- RIPEMD-160 -------------------------------------------------------------------

struct Rmd160 {
    uint32_t h[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    uint8_t buf[64];
    size_t used = 0;
    uint64_t total = 0;

    static uint32_t f(int j, uint32_t x, uint32_t y, uint32_t z)
    {
        switch (j / 16) {
        case 0: return x ^ y ^ z;
        case 1: return (x & y) | (~x & z);
        case 2: return (x | ~y) ^ z;
        case 3: return (x & z) | (y & ~z);
        default: return x ^ (y | ~z);
        }
    }

    void transform(const uint8_t *block)
    {
        static const int r[80] = {0, 1, 2,  3,  4,  5,  6,  7, 8,  9,  10, 11, 12, 13, 14, 15, 7, 4,  13, 1,
                                  10, 6, 15, 3,  12, 0,  9,  5, 2,  14, 11, 8,  3,  10, 14, 4, 9, 15, 8,  1,
                                  2, 7, 0,  6,  13, 11, 5,  12, 1, 9,  11, 10, 0,  8,  12, 4, 13, 3, 7,  15,
                                  14, 5, 6, 2,  4,  0,  5,  9, 7,  12, 2,  10, 14, 1,  3,  8, 11, 6, 15, 13};
        static const int rp[80] = {5,  14, 7,  0, 9, 2,  11, 4,  13, 6,  15, 8,  1,  10, 3,  12, 6, 11, 3, 7,
                                   0,  13, 5,  10, 14, 15, 8, 12, 4, 9,  1,  2,  15, 5,  1,  3,  7,  14, 6, 9,
                                   11, 8,  12, 2, 10, 0,  4, 13, 8,  6,  4,  1,  3,  11, 15, 0, 5,  12, 2, 13,
                                   9,  7,  10, 14, 12, 15, 10, 4, 1,  5,  8,  7,  6,  2,  13, 14, 0, 3, 9, 11};
        static const int s[80] = {11, 14, 15, 12, 5,  8,  7,  9,  11, 13, 14, 15, 6,  7,  9,  8,  7,  6,  8,  13,
                                  11, 9,  7,  15, 7,  12, 15, 9,  11, 7,  13, 12, 11, 13, 6,  7,  14, 9,  13, 15,
                                  14, 8,  13, 6,  5,  12, 7,  5,  11, 12, 14, 15, 14, 15, 9,  8,  9,  14, 5,  6,
                                  8,  6,  5,  12, 9,  15, 5,  11, 6,  8,  13, 12, 5,  12, 13, 14, 11, 8,  5,  6};
        static const int sp[80] = {8,  9,  9,  11, 13, 15, 15, 5,  7,  7,  8,  11, 14, 14, 12, 6,  9,  13, 15, 7,
                                   12, 8,  9,  11, 7,  7,  12, 7,  6,  15, 13, 11, 9,  7,  15, 11, 8,  6,  6,  14,
                                   12, 13, 5,  14, 13, 13, 7,  5,  15, 5,  8,  11, 14, 14, 6,  14, 6,  9,  12, 9,
                                   12, 5,  15, 8,  8,  5,  12, 9,  12, 5,  14, 6,  8,  13, 6,  5,  15, 13, 11, 11};
        static const uint32_t K[5] = {0x00000000, 0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xa953fd4e};
        static const uint32_t KP[5] = {0x50a28be6, 0x5c4dd124, 0x6d703ef3, 0x7a6d76e9, 0x00000000};
        uint32_t x[16];
        for (int i = 0; i < 16; ++i)
            x[i] = load_le(block + 4 * i);
        uint32_t al = h[0], bl = h[1], cl = h[2], dl = h[3], el = h[4];
        uint32_t ar = al, br = bl, cr = cl, dr = dl, er = el;
        for (int j = 0; j < 80; ++j) {
            uint32_t t = rol(al + f(j, bl, cl, dl) + x[r[j]] + K[j / 16], s[j]) + el;
            al = el; el = dl; dl = rol(cl, 10); cl = bl; bl = t;
            t = rol(ar + f(79 - j, br, cr, dr) + x[rp[j]] + KP[j / 16], sp[j]) + er;
            ar = er; er = dr; dr = rol(cr, 10); cr = br; br = t;
        }
        uint32_t t = h[1] + cl + dr;
        h[1] = h[2] + dl + er;
        h[2] = h[3] + el + ar;
        h[3] = h[4] + al + br;
        h[4] = h[0] + bl + cr;
        h[0] = t;
    }

    void update(const uint8_t *p, size_t n)
    {
        total += n;
        while (n) {
            size_t take = std::min(n, sizeof buf - used);
            std::memcpy(buf + used, p, take);
            used += take;
            p += take;
            n -= take;
            if (used == 64) {
                transform(buf);
                used = 0;
            }
        }
    }

    Bytes final()
    {
        uint64_t bits = total * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        uint8_t zero = 0;
        while (used != 56)
            update(&zero, 1);
        uint8_t len[8];
        store_le(len, uint32_t(bits));
        store_le(len + 4, uint32_t(bits >> 32));
        update(len, 8);
        Bytes out(20);
        for (int i = 0; i < 5; ++i)
            store_le(out.data() + 4 * i, h[i]);
        return out;
    }
};

// ---- Digest adapters -------------------------------------------------------------

class Md2Digest : public Digest {
    SEC_MD2_CTX c;
public:
    Md2Digest() { sec_MD2Init(&c); }
    void update(const uint8_t *p, size_t n) override { sec_MD2Update(&c, const_cast<uint8_t *>(p), sec_uint4(n)); }
    Bytes final() override
    {
        Bytes d(16);
        sec_MD2Final(d.data(), &c);
        return d;
    }
};

class Md4Digest : public Digest {
    SEC_MD4_CTX c;
public:
    Md4Digest() { md_init(&c); }
    void update(const uint8_t *p, size_t n) override { md_update<SEC_MD4_CTX, md4_transform>(&c, p, n); }
    Bytes final() override
    {
        Bytes d(16);
        md_final<SEC_MD4_CTX, md4_transform>(d.data(), &c);
        return d;
    }
};

class Md5Digest : public Digest {
    SEC_MD5_CTX c;
public:
    Md5Digest() { md_init(&c); }
    void update(const uint8_t *p, size_t n) override { md_update<SEC_MD5_CTX, md5_transform>(&c, p, n); }
    Bytes final() override
    {
        Bytes d(16);
        md_final<SEC_MD5_CTX, md5_transform>(d.data(), &c);
        return d;
    }
};

class ShsDigest : public Digest {
    SHS_INFO s;
    bool sha1;
public:
    explicit ShsDigest(bool is_sha1) : sha1(is_sha1) { shs_init(&s); }
    void update(const uint8_t *p, size_t n) override { shs_update(&s, p, n, sha1); }
    Bytes final() override
    {
        shs_final(&s, sha1);
        Bytes d(20);
        for (int i = 0; i < 5; ++i)
            store_be(d.data() + 4 * i, s.digest[i]);
        return d;
    }
};

class RmdDigest : public Digest {
    Rmd160 r;
public:
    void update(const uint8_t *p, size_t n) override { r.update(p, n); }
    Bytes final() override { return r.final(); }
};

} // namespace

std::unique_ptr<Digest> make_digest(Hash h)
{
    switch (h) {
    case Hash::MD2: return std::make_unique<Md2Digest>();
    case Hash::MD4: return std::make_unique<Md4Digest>();
    case Hash::MD5: return std::make_unique<Md5Digest>();
    case Hash::SHA0: return std::make_unique<ShsDigest>(false);
    case Hash::SHA1: return std::make_unique<ShsDigest>(true);
    case Hash::RIPEMD160: return std::make_unique<RmdDigest>();
    default: return nullptr;
    }
}

} // namespace legacy
} // namespace compat

using namespace compat::legacy;

extern "C" {

void sec_MD2Init(SEC_MD2_CTX *context)
{
    std::memset(context, 0, sizeof *context);
}

void sec_MD2Update(SEC_MD2_CTX *context, unsigned char *input, sec_uint4 inputLen)
{
    if (inputLen == 0)
        return;
    unsigned index = context->count;
    context->count = (index + inputLen) & 0xf;
    unsigned part = 16 - index;
    unsigned i = 0;
    if (inputLen >= part) {
        std::memcpy(context->buffer + index, input, part);
        md2_transform(context, context->buffer);
        for (i = part; i + 15 < inputLen; i += 16)
            md2_transform(context, input + i);
        index = 0;
    }
    std::memcpy(context->buffer + index, input + i, inputLen - i);
}

void sec_MD2Final(unsigned char *digest, SEC_MD2_CTX *context)
{
    unsigned char padding[16];
    unsigned padlen = 16 - context->count;
    std::memset(padding, int(padlen), padlen);
    sec_MD2Update(context, padding, padlen);
    unsigned char checksum[16];
    std::memcpy(checksum, context->checksum, 16);
    sec_MD2Update(context, checksum, 16);
    std::memcpy(digest, context->state, 16);
    std::memset(context, 0, sizeof *context);
}

void sec_MD4Init(SEC_MD4_CTX *context) { md_init(context); }

void sec_MD4Update(SEC_MD4_CTX *context, unsigned char *input, sec_uint4 inputLen)
{
    md_update<SEC_MD4_CTX, md4_transform>(context, input, inputLen);
}

void sec_MD4Final(unsigned char *digest, SEC_MD4_CTX *context)
{
    md_final<SEC_MD4_CTX, md4_transform>(digest, context);
}

void sec_MD5Init(SEC_MD5_CTX *context) { md_init(context); }

void sec_MD5Update(SEC_MD5_CTX *context, unsigned char *input, sec_uint4 inputLen)
{
    md_update<SEC_MD5_CTX, md5_transform>(context, input, inputLen);
}

void sec_MD5Final(unsigned char *digest, SEC_MD5_CTX *context)
{
    md_final<SEC_MD5_CTX, md5_transform>(digest, context);
}

// After shsFinal()/shs1Final() the message digest is in shsInfo->digest[0..4]
// as host-order words (big-endian when serialised).
void shsInit(SHS_INFO *shsInfo) { shs_init(shsInfo); }

void shsUpdate(SHS_INFO *shsInfo, unsigned char *buffer, sec_int4 count)
{
    if (count > 0)
        shs_update(shsInfo, buffer, size_t(count), false);
}

void shsFinal(SHS_INFO *shsInfo) { shs_final(shsInfo, false); }

void shs1Init(SHS_INFO *shsInfo) { shs_init(shsInfo); }

void shs1Update(SHS_INFO *shsInfo, unsigned char *buffer, sec_int4 count)
{
    if (count > 0)
        shs_update(shsInfo, buffer, size_t(count), true);
}

void shs1Final(SHS_INFO *shsInfo) { shs_final(shsInfo, true); }

} // extern "C"

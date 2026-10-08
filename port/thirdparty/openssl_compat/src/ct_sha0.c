/* SHA-0 (FIPS 180, 1993) with the OpenSSL 1.0.1 SHA_* interface. */
#include <stddef.h>
#include <string.h>

#include "OpenSSL/sha.h"

#define ROL32(x, n) ((((x) << (n)) | ((x) >> (32 - (n)))) & 0xffffffffU)

void SHA_Transform(SHA_CTX *c, const unsigned char *p)
{
    SHA_LONG w[80];
    SHA_LONG a, b, d, e, f, k, t, cc;
    int i;

    for (i = 0; i < 16; i++, p += 4)
        w[i] = ((SHA_LONG)p[0] << 24) | ((SHA_LONG)p[1] << 16) | ((SHA_LONG)p[2] << 8) | (SHA_LONG)p[3];
    for (i = 16; i < 80; i++)
        w[i] = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];

    a = c->h0;
    b = c->h1;
    cc = c->h2;
    d = c->h3;
    e = c->h4;
    for (i = 0; i < 80; i++) {
        if (i < 20) {
            f = (b & cc) | (~b & d);
            k = 0x5a827999U;
        } else if (i < 40) {
            f = b ^ cc ^ d;
            k = 0x6ed9eba1U;
        } else if (i < 60) {
            f = (b & cc) | (b & d) | (cc & d);
            k = 0x8f1bbcdcU;
        } else {
            f = b ^ cc ^ d;
            k = 0xca62c1d6U;
        }
        t = (ROL32(a, 5) + f + e + k + w[i]) & 0xffffffffU;
        e = d;
        d = cc;
        cc = ROL32(b, 30);
        b = a;
        a = t;
    }
    c->h0 = (c->h0 + a) & 0xffffffffU;
    c->h1 = (c->h1 + b) & 0xffffffffU;
    c->h2 = (c->h2 + cc) & 0xffffffffU;
    c->h3 = (c->h3 + d) & 0xffffffffU;
    c->h4 = (c->h4 + e) & 0xffffffffU;
}

int SHA_Init(SHA_CTX *c)
{
    memset(c, 0, sizeof(*c));
    c->h0 = 0x67452301U;
    c->h1 = 0xefcdab89U;
    c->h2 = 0x98badcfeU;
    c->h3 = 0x10325476U;
    c->h4 = 0xc3d2e1f0U;
    return 1;
}

int SHA_Update(SHA_CTX *c, const void *data_, size_t len)
{
    const unsigned char *data = (const unsigned char *)data_;
    unsigned char *buf = (unsigned char *)c->data;
    SHA_LONG lo = (c->Nl + ((SHA_LONG)len << 3)) & 0xffffffffU;

    if (lo < c->Nl)
        c->Nh++;
    c->Nh += (SHA_LONG)((unsigned long long)len >> 29);
    c->Nl = lo;

    if (c->num != 0) {
        size_t take = SHA_CBLOCK - c->num;
        if (len < take) {
            memcpy(buf + c->num, data, len);
            c->num += (unsigned int)len;
            return 1;
        }
        memcpy(buf + c->num, data, take);
        SHA_Transform(c, buf);
        data += take;
        len -= take;
        c->num = 0;
    }
    while (len >= SHA_CBLOCK) {
        SHA_Transform(c, data);
        data += SHA_CBLOCK;
        len -= SHA_CBLOCK;
    }
    memcpy(buf, data, len);
    c->num = (unsigned int)len;
    return 1;
}

int SHA_Final(unsigned char *md, SHA_CTX *c)
{
    unsigned char *buf = (unsigned char *)c->data;
    SHA_LONG h[5];
    unsigned int n = c->num;
    int i;

    buf[n++] = 0x80;
    if (n > SHA_LAST_BLOCK) {
        memset(buf + n, 0, SHA_CBLOCK - n);
        SHA_Transform(c, buf);
        n = 0;
    }
    memset(buf + n, 0, SHA_LAST_BLOCK - n);
    for (i = 0; i < 4; i++) {
        buf[SHA_LAST_BLOCK + i] = (unsigned char)(c->Nh >> (24 - 8 * i));
        buf[SHA_LAST_BLOCK + 4 + i] = (unsigned char)(c->Nl >> (24 - 8 * i));
    }
    SHA_Transform(c, buf);

    h[0] = c->h0;
    h[1] = c->h1;
    h[2] = c->h2;
    h[3] = c->h3;
    h[4] = c->h4;
    for (i = 0; i < 5; i++) {
        md[4 * i] = (unsigned char)(h[i] >> 24);
        md[4 * i + 1] = (unsigned char)(h[i] >> 16);
        md[4 * i + 2] = (unsigned char)(h[i] >> 8);
        md[4 * i + 3] = (unsigned char)h[i];
    }
    memset(c, 0, sizeof(*c));
    return 1;
}

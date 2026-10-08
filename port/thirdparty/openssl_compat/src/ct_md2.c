/* MD2 message digest (RFC 1319) with the OpenSSL 1.0.1 MD2_* interface. */
#include <stddef.h>
#include <string.h>

#include "OpenSSL/md2.h"

static const unsigned char PI_SUBST[256] = {
    41, 46, 67, 201, 162, 216, 124, 1, 61, 54, 84, 161, 236, 240, 6, 19,
    98, 167, 5, 243, 192, 199, 115, 140, 152, 147, 43, 217, 188, 76, 130, 202,
    30, 155, 87, 60, 253, 212, 224, 22, 103, 66, 111, 24, 138, 23, 229, 18,
    190, 78, 196, 214, 218, 158, 222, 73, 160, 251, 245, 142, 187, 47, 238, 122,
    169, 104, 121, 145, 21, 178, 7, 63, 148, 194, 16, 137, 11, 34, 95, 33,
    128, 127, 93, 154, 90, 144, 50, 39, 53, 62, 204, 231, 191, 247, 151, 3,
    255, 25, 48, 179, 72, 165, 181, 209, 215, 94, 146, 42, 172, 86, 170, 198,
    79, 184, 56, 210, 150, 164, 125, 182, 118, 252, 107, 226, 156, 116, 4, 241,
    69, 157, 112, 89, 100, 113, 135, 32, 134, 91, 207, 101, 230, 45, 168, 2,
    27, 96, 37, 173, 174, 176, 185, 246, 28, 70, 97, 105, 52, 64, 126, 15,
    85, 71, 163, 35, 221, 81, 175, 58, 195, 92, 249, 206, 186, 197, 234, 38,
    44, 83, 13, 110, 133, 40, 132, 9, 211, 223, 205, 244, 65, 129, 77, 82,
    106, 220, 55, 200, 108, 193, 171, 250, 36, 225, 123, 8, 12, 189, 177, 74,
    120, 136, 149, 139, 227, 99, 232, 109, 233, 203, 213, 254, 59, 0, 29, 57,
    242, 239, 183, 14, 102, 88, 208, 228, 166, 119, 114, 248, 235, 117, 75, 10,
    49, 68, 80, 180, 143, 237, 31, 26, 219, 153, 141, 51, 159, 17, 131, 20
};

static void md2_block(MD2_CTX *c, const unsigned char *block)
{
    unsigned char x[48];
    unsigned int j, k, t;
    unsigned int l;

    for (j = 0; j < 16; j++) {
        x[j] = (unsigned char)c->state[j];
        x[16 + j] = block[j];
        x[32 + j] = (unsigned char)(c->state[j] ^ block[j]);
    }
    t = 0;
    for (j = 0; j < 18; j++) {
        for (k = 0; k < 48; k++)
            t = x[k] ^= PI_SUBST[t];
        t = (t + j) & 0xff;
    }
    for (j = 0; j < 16; j++)
        c->state[j] = x[j];

    l = c->cksm[15];
    for (j = 0; j < 16; j++) {
        c->cksm[j] ^= PI_SUBST[block[j] ^ l];
        c->cksm[j] &= 0xff;
        l = c->cksm[j];
    }
    memset(x, 0, sizeof(x));
}

const char *MD2_options(void)
{
    return "md2(int)";
}

int MD2_Init(MD2_CTX *c)
{
    memset(c, 0, sizeof(*c));
    return 1;
}

int MD2_Update(MD2_CTX *c, const unsigned char *data, size_t len)
{
    if (len == 0)
        return 1;
    if (c->num != 0) {
        size_t take = MD2_BLOCK - c->num;
        if (len < take) {
            memcpy(c->data + c->num, data, len);
            c->num += (unsigned int)len;
            return 1;
        }
        memcpy(c->data + c->num, data, take);
        md2_block(c, c->data);
        data += take;
        len -= take;
        c->num = 0;
    }
    while (len >= MD2_BLOCK) {
        md2_block(c, data);
        data += MD2_BLOCK;
        len -= MD2_BLOCK;
    }
    memcpy(c->data, data, len);
    c->num = (unsigned int)len;
    return 1;
}

int MD2_Final(unsigned char *md, MD2_CTX *c)
{
    unsigned char pad = (unsigned char)(MD2_BLOCK - c->num);
    unsigned char cksm[MD2_BLOCK];
    unsigned int i;

    memset(c->data + c->num, pad, pad);
    md2_block(c, c->data);
    for (i = 0; i < MD2_BLOCK; i++)
        cksm[i] = (unsigned char)c->cksm[i];
    md2_block(c, cksm);
    for (i = 0; i < MD2_DIGEST_LENGTH; i++)
        md[i] = (unsigned char)c->state[i];
    memset(c, 0, sizeof(*c));
    return 1;
}

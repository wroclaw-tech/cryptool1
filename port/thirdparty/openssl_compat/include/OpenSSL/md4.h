/*
 * OpenSSL 1.0.1/3.x compatible MD4 declarations (implemented by libcrypto).
 * Same layout and guards as OpenSSL 3's <openssl/md4.h>.
 */
#ifndef OPENSSL_MD4_H
#define OPENSSL_MD4_H
#ifndef HEADER_MD4_H
#define HEADER_MD4_H
#endif

#include "ct_ossl_base.h"

#define MD4_DIGEST_LENGTH 16
#define MD4_LONG unsigned int
#define MD4_CBLOCK 64
#define MD4_LBLOCK (MD4_CBLOCK / 4)

CT_OSSL_BEGIN_C

typedef struct MD4state_st {
    MD4_LONG A, B, C, D;
    MD4_LONG Nl, Nh;
    MD4_LONG data[MD4_LBLOCK];
    unsigned int num;
} MD4_CTX;

int MD4_Init(MD4_CTX *c);
int MD4_Update(MD4_CTX *c, const void *data, CT_OSSL_SIZE_T len);
int MD4_Final(unsigned char *md, MD4_CTX *c);
unsigned char *MD4(const unsigned char *d, CT_OSSL_SIZE_T n, unsigned char *md);
void MD4_Transform(MD4_CTX *c, const unsigned char *b);

CT_OSSL_END_C

#endif

/*
 * OpenSSL 1.0.1/3.x compatible RIPEMD-160 declarations (implemented by libcrypto).
 * Same layout and guards as OpenSSL 3's <openssl/ripemd.h>.
 */
#ifndef OPENSSL_RIPEMD_H
#define OPENSSL_RIPEMD_H
#ifndef HEADER_RIPEMD_H
#define HEADER_RIPEMD_H
#endif

#include "ct_ossl_base.h"

#define RIPEMD160_DIGEST_LENGTH 20
#define RIPEMD160_LONG unsigned int
#define RIPEMD160_CBLOCK 64
#define RIPEMD160_LBLOCK (RIPEMD160_CBLOCK / 4)

CT_OSSL_BEGIN_C

typedef struct RIPEMD160state_st {
    RIPEMD160_LONG A, B, C, D, E;
    RIPEMD160_LONG Nl, Nh;
    RIPEMD160_LONG data[RIPEMD160_LBLOCK];
    unsigned int num;
} RIPEMD160_CTX;

int RIPEMD160_Init(RIPEMD160_CTX *c);
int RIPEMD160_Update(RIPEMD160_CTX *c, const void *data, CT_OSSL_SIZE_T len);
int RIPEMD160_Final(unsigned char *md, RIPEMD160_CTX *c);
unsigned char *RIPEMD160(const unsigned char *d, CT_OSSL_SIZE_T n, unsigned char *md);
void RIPEMD160_Transform(RIPEMD160_CTX *c, const unsigned char *b);

CT_OSSL_END_C

#endif

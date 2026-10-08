/*
 * OpenSSL 1.0.1/3.x compatible MD5 declarations (implemented by libcrypto).
 * Same layout and guards as OpenSSL 3's <openssl/md5.h>.
 */
#ifndef OPENSSL_MD5_H
#define OPENSSL_MD5_H
#ifndef HEADER_MD5_H
#define HEADER_MD5_H
#endif

#include "ct_ossl_base.h"

#define MD5_DIGEST_LENGTH 16
#define MD5_LONG unsigned int
#define MD5_CBLOCK 64
#define MD5_LBLOCK (MD5_CBLOCK / 4)

CT_OSSL_BEGIN_C

typedef struct MD5state_st {
    MD5_LONG A, B, C, D;
    MD5_LONG Nl, Nh;
    MD5_LONG data[MD5_LBLOCK];
    unsigned int num;
} MD5_CTX;

int MD5_Init(MD5_CTX *c);
int MD5_Update(MD5_CTX *c, const void *data, CT_OSSL_SIZE_T len);
int MD5_Final(unsigned char *md, MD5_CTX *c);
unsigned char *MD5(const unsigned char *d, CT_OSSL_SIZE_T n, unsigned char *md);
void MD5_Transform(MD5_CTX *c, const unsigned char *b);

CT_OSSL_END_C

#endif

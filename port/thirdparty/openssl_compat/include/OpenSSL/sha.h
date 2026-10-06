/*
 * OpenSSL 1.0.1/3.x compatible SHA declarations. SHA-1/224/256/384/512 are
 * implemented by libcrypto (same layout and guards as OpenSSL 3's
 * <openssl/sha.h>); SHA-0 (SHA_Init/SHA_Update/SHA_Final), removed in
 * OpenSSL 1.1.0, is implemented by cryptool_openssl_compat (ct_SHA0_*).
 */
#ifndef OPENSSL_SHA_H
#define OPENSSL_SHA_H
#ifndef HEADER_SHA_H
#define HEADER_SHA_H
#endif

#include "ct_ossl_base.h"

#define SHA_DIGEST_LENGTH 20
#define SHA_LONG unsigned int
#define SHA_LBLOCK 16
#define SHA_CBLOCK (SHA_LBLOCK * 4)
#define SHA_LAST_BLOCK (SHA_CBLOCK - 8)

#define SHA256_CBLOCK (SHA_LBLOCK * 4)
#define SHA256_192_DIGEST_LENGTH 24
#define SHA224_DIGEST_LENGTH 28
#define SHA256_DIGEST_LENGTH 32
#define SHA384_DIGEST_LENGTH 48
#define SHA512_DIGEST_LENGTH 64
#define SHA512_CBLOCK (SHA_LBLOCK * 8)
#if (defined(_WIN32) || defined(_WIN64)) && !defined(__MINGW32__)
#define SHA_LONG64 unsigned __int64
#elif defined(__arch64__)
#define SHA_LONG64 unsigned long
#else
#define SHA_LONG64 unsigned long long
#endif

CT_OSSL_BEGIN_C

typedef struct SHAstate_st {
    SHA_LONG h0, h1, h2, h3, h4;
    SHA_LONG Nl, Nh;
    SHA_LONG data[SHA_LBLOCK];
    unsigned int num;
} SHA_CTX;

typedef struct SHA256state_st {
    SHA_LONG h[8];
    SHA_LONG Nl, Nh;
    SHA_LONG data[SHA_LBLOCK];
    unsigned int num, md_len;
} SHA256_CTX;

typedef struct SHA512state_st {
    SHA_LONG64 h[8];
    SHA_LONG64 Nl, Nh;
    union {
        SHA_LONG64 d[SHA_LBLOCK];
        unsigned char p[SHA512_CBLOCK];
    } u;
    unsigned int num, md_len;
} SHA512_CTX;

#define SHA_Init ct_SHA0_Init
#define SHA_Update ct_SHA0_Update
#define SHA_Final ct_SHA0_Final
#define SHA_Transform ct_SHA0_Transform

int SHA_Init(SHA_CTX *c);
int SHA_Update(SHA_CTX *c, const void *data, CT_OSSL_SIZE_T len);
int SHA_Final(unsigned char *md, SHA_CTX *c);
void SHA_Transform(SHA_CTX *c, const unsigned char *data);

int SHA1_Init(SHA_CTX *c);
int SHA1_Update(SHA_CTX *c, const void *data, CT_OSSL_SIZE_T len);
int SHA1_Final(unsigned char *md, SHA_CTX *c);
void SHA1_Transform(SHA_CTX *c, const unsigned char *data);
unsigned char *SHA1(const unsigned char *d, CT_OSSL_SIZE_T n, unsigned char *md);

int SHA224_Init(SHA256_CTX *c);
int SHA224_Update(SHA256_CTX *c, const void *data, CT_OSSL_SIZE_T len);
int SHA224_Final(unsigned char *md, SHA256_CTX *c);
int SHA256_Init(SHA256_CTX *c);
int SHA256_Update(SHA256_CTX *c, const void *data, CT_OSSL_SIZE_T len);
int SHA256_Final(unsigned char *md, SHA256_CTX *c);
void SHA256_Transform(SHA256_CTX *c, const unsigned char *data);
unsigned char *SHA224(const unsigned char *d, CT_OSSL_SIZE_T n, unsigned char *md);
unsigned char *SHA256(const unsigned char *d, CT_OSSL_SIZE_T n, unsigned char *md);

int SHA384_Init(SHA512_CTX *c);
int SHA384_Update(SHA512_CTX *c, const void *data, CT_OSSL_SIZE_T len);
int SHA384_Final(unsigned char *md, SHA512_CTX *c);
int SHA512_Init(SHA512_CTX *c);
int SHA512_Update(SHA512_CTX *c, const void *data, CT_OSSL_SIZE_T len);
int SHA512_Final(unsigned char *md, SHA512_CTX *c);
void SHA512_Transform(SHA512_CTX *c, const unsigned char *data);
unsigned char *SHA384(const unsigned char *d, CT_OSSL_SIZE_T n, unsigned char *md);
unsigned char *SHA512(const unsigned char *d, CT_OSSL_SIZE_T n, unsigned char *md);

CT_OSSL_END_C

#endif

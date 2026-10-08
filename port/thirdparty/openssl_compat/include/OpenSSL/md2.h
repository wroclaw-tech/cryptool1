/*
 * OpenSSL 1.0.1 compatible MD2 API. OpenSSL 3 ships without MD2, so the
 * functions are implemented by cryptool_openssl_compat (ct_MD2_*).
 */
#ifndef OPENSSL_MD2_H
#define OPENSSL_MD2_H
#ifndef HEADER_MD2_H
#define HEADER_MD2_H
#endif

#include "ct_ossl_base.h"

#define MD2_DIGEST_LENGTH 16
#define MD2_BLOCK 16
#ifndef MD2_INT
#define MD2_INT unsigned int
#endif

CT_OSSL_BEGIN_C

typedef struct MD2state_st {
    unsigned int num;
    unsigned char data[MD2_BLOCK];
    MD2_INT cksm[MD2_BLOCK];
    MD2_INT state[MD2_BLOCK];
} MD2_CTX;

#define MD2_options ct_MD2_options
#define MD2_Init ct_MD2_Init
#define MD2_Update ct_MD2_Update
#define MD2_Final ct_MD2_Final

const char *MD2_options(void);
int MD2_Init(MD2_CTX *c);
int MD2_Update(MD2_CTX *c, const unsigned char *data, CT_OSSL_SIZE_T len);
int MD2_Final(unsigned char *md, MD2_CTX *c);

CT_OSSL_END_C

#endif

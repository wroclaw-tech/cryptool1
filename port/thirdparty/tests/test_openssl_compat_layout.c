/* Compiled against the real OpenSSL 3 headers only (no compat include dir). */
#include <stddef.h>

#include <openssl/crypto.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/ripemd.h>
#include <openssl/sha.h>

#define CT_LAYOUT(T, last) \
    const size_t ct_real_sizeof_##T = sizeof(T); \
    const size_t ct_real_offsetof_##T = offsetof(T, last);

CT_LAYOUT(MD4_CTX, num)
CT_LAYOUT(MD5_CTX, num)
CT_LAYOUT(RIPEMD160_CTX, num)
CT_LAYOUT(SHA_CTX, num)
CT_LAYOUT(SHA256_CTX, md_len)
CT_LAYOUT(SHA512_CTX, md_len)

const char *ct_real_openssl_version(void)
{
    return OpenSSL_version(OPENSSL_VERSION);
}

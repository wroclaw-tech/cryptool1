/*
 * Minimal stand-in for OpenSSL 1.0.1's "crypto.h" as included bare by
 * CrypTool/DlgAbout.cpp inside namespace OPENSSL { }: only the version query,
 * forwarded to OpenSSL 3's OpenSSL_version(). No system #includes.
 */
#ifndef CT_OSSL_COMPAT_CRYPTO_H
#define CT_OSSL_COMPAT_CRYPTO_H

#ifndef OPENSSL_VERSION
#define OPENSSL_VERSION 0
#endif
#ifndef SSLEAY_VERSION
#define SSLEAY_VERSION OPENSSL_VERSION
#endif
#ifndef SSLeay_version
#define SSLeay_version OpenSSL_version
#endif
#ifndef SSLeay
#define SSLeay OpenSSL_version_num
#endif

#ifdef __cplusplus
extern "C" {
#endif

const char *OpenSSL_version(int type);
unsigned long OpenSSL_version_num(void);

#ifdef __cplusplus
}
#endif

#endif

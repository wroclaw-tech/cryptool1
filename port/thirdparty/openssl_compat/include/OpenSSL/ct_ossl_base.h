/*
 * Shared helpers for the OpenSSL 1.0.1 compatibility headers used by CrypTool.
 * Deliberately free of system #includes: CrypTool includes these headers inside
 * C++ namespaces (namespace __SSL { ... }), where pulling in system headers
 * would misplace their declarations.
 */
#ifndef CT_OSSL_BASE_H
#define CT_OSSL_BASE_H

#if defined(__SIZE_TYPE__)
#define CT_OSSL_SIZE_T __SIZE_TYPE__
#else
#define CT_OSSL_SIZE_T size_t
#endif

#ifdef __cplusplus
#define CT_OSSL_BEGIN_C extern "C" {
#define CT_OSSL_END_C }
#else
#define CT_OSSL_BEGIN_C
#define CT_OSSL_END_C
#endif

#endif

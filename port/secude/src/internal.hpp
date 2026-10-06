#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <openssl/bn.h>
#include <openssl/evp.h>

// SECUDE's AlgEnc/AlgHash enumerators (RSA, DSA, SHA1, ...) clash with OpenSSL names,
// and secude/bio.h, secude/ssl.h re-declare OpenSSL's BIO/SSL API. The SSL
// part of SECUDE is not needed, so those headers are suppressed.
#ifndef HEADER_BIO_H
#define HEADER_BIO_H
#endif
#define HEADER_SSL_H
#define HEADER_SSL3_H
#define HEADER_SSL23_H
typedef SSL *H_SSL;
#define RSA SECUDE_ALG_RSA
#define DSA SECUDE_ALG_DSA
#define MD2 SECUDE_HASH_MD2
#define MD4 SECUDE_HASH_MD4
#define MD5 SECUDE_HASH_MD5
#define SHA SECUDE_HASH_SHA
#define SHA1 SECUDE_HASH_SHA1
#define RIPEMD160 SECUDE_HASH_RIPEMD160
#include <secude/secure.h>
#include <secude/asn1.h>
#include <secude/af.h>
#include <secude/pkcs.h>
#undef RSA
#undef DSA
#undef MD2
#undef MD4
#undef MD5
#undef SHA
#undef SHA1
#undef RIPEMD160

// arithmet.h maps short names (div, add, comp, ...) to arithmetic_*; keep them out of C++ code.
#undef div
#undef madd
#undef msub
#undef mmult
#undef mdiv
#undef mexp
#undef mexp2
#undef cadd
#undef add
#undef sub
#undef mult
#undef dmult
#undef shift
#undef wshift
#undef comp
#undef normalize
#undef trans

#include "secude_compat.h"

extern "C" {
// Used by CrypTool but not declared by the headers it includes.
OctetString *aux_alloc_OctetString(void);
extern AlgId md2WithRSASignature_aid;
}

namespace compat {

using Bytes = std::vector<uint8_t>;

// ---- memory -------------------------------------------------------------
// Every block handed out to callers is registered; freeing an unknown
// pointer is ignored (CrypTool occasionally passes stack or foreign memory).
void *mem_alloc(size_t n);
void *mem_realloc(void *p, size_t n);
void mem_free(void *p);
bool mem_owned(const void *p);
char *mem_strdup(const char *s);
char *mem_strdup(const std::string &s);

template <class T> T *mem_new() { return static_cast<T *>(mem_alloc(sizeof(T))); }

OctetString *new_ostr(const uint8_t *data, size_t len);
inline OctetString *new_ostr(const Bytes &b) { return new_ostr(b.data(), b.size()); }
void fill_ostr(OctetString *o, const uint8_t *data, size_t len);
inline void fill_ostr(OctetString *o, const Bytes &b) { fill_ostr(o, b.data(), b.size()); }
BitString *new_bstr(const Bytes &b, size_t nbits);
void fill_bstr(BitString *o, const Bytes &b, size_t nbits);
Bytes bytes_of(const OctetString *o);
Bytes bytes_of(const BitString *b);
Bytes bytes_of(const char *s);

// ---- errors ---------------------------------------------------------------
struct Error : std::runtime_error {
    int code;
    Error(int c, const std::string &msg) : std::runtime_error(msg), code(c) {}
};
[[noreturn]] void fail(int code, const std::string &text = std::string());
void push_error(int code, const char *proc, const std::string &text);
const char *error_name(int code);

// Runs f() and converts exceptions into SECUDE errors (returned value: onfail).
template <class R, class F> R guarded(const char *proc, R onfail, F &&f)
{
    try {
        return f();
    } catch (const Error &e) {
        push_error(e.code, proc, e.what());
    } catch (const std::bad_alloc &) {
        push_error(EMALLOC, proc, "out of memory");
    } catch (const std::exception &e) {
        push_error(EINVALID, proc, e.what());
    }
    return onfail;
}

template <class F> void guarded_void(const char *proc, F &&f)
{
    guarded<int>(proc, -1, [&] { f(); return 0; });
}

// ---- OIDs and algorithms --------------------------------------------------
using Oid = std::vector<unsigned>;
Oid oid_of(const ObjId *o);
ObjId *new_objid(const Oid &o);
void fill_objid(ObjId *dst, const Oid &o);
std::string oid_to_string(const Oid &o);
bool oid_equal(const ObjId *a, const ObjId *b);
bool oid_equal(const ObjId *a, const Oid &b);

enum class Cipher { None, DES, DES3, IDEA, RC2, RC4, AES };
enum class Pad { None, Std, Pkcs };
enum class Hash { None, MD2, MD4, MD5, SHA0, SHA1, RIPEMD160, SHA256, SHA384, SHA512 };

struct AlgInfo {
    const char *name;
    Oid oid;
    ParmType parm;
    AlgType type;
    AlgEnc enc;
    Hash hash;
    Cipher cipher;
    AlgMode mode;
    Pad pad;
    AlgId *global;
};

const AlgInfo *alg_by_oid(const Oid &o);
const AlgInfo *alg_by_oid(const ObjId *o);
const AlgInfo *alg_by_name(const char *name);
std::string alg_display_name(const ObjId *o);
ObjId *pse_object_oid(const char *name);
const char *pse_object_name(const ObjId *oid);

// Well-known OIDs (values are documented in README.md).
namespace oids {
extern const Oid rsa, rsaEncryption, dsa, id_dsa;
extern const Oid md2, md4, md5, sha, sha1, ripemd160, sha256, sha384, sha512;
extern const Oid data, encryptedData;
extern const Oid keyBag, pkcs8ShroudedKeyBag, certBag, x509Certificate;
extern const Oid friendlyName, localKeyId;
extern const Oid pbeSHA1RC4_128, pbeSHA1RC4_40, pbeSHA1DES3, pbeSHA1DES2, pbeSHA1RC2_128, pbeSHA1RC2_40;
extern const Oid pbes2, pbkdf2, hmacSHA1, hmacSHA256, hmacSHA384, hmacSHA512;
extern const Oid aes128CBC, aes192CBC, aes256CBC, desEDE3CBC;
extern const Oid subjectKeyIdentifier, keyUsage, basicConstraints, authorityKeyIdentifier;
extern const Oid cryptoolPseName;
} // namespace oids

// ---- deep copy / free of SECUDE structures --------------------------------
ObjId *copy_objid(const ObjId *o);
void free_objid_content(ObjId *o);
AlgId *copy_algid(const AlgId *a);
void free_algid_content(AlgId *a);
void free_algid(AlgId *a);
KeyBits *copy_keybits(const KeyBits *k);
void free_keybits_content(KeyBits *k);
KeyInfo *copy_keyinfo(const KeyInfo *k);
void free_keyinfo_content(KeyInfo *k);
void free_keyinfo(KeyInfo *k);
DName *copy_dname(const DName *d);
void free_dname(DName *d);
Validity *copy_validity(const Validity *v);
void free_validity(Validity *v);
Signature *copy_signature(const Signature *s);
void free_signature(Signature *s);
CertExtensions *copy_extensions(const CertExtensions *e);
void free_extensions(CertExtensions *e);
ToBeSigned *copy_tbs(const ToBeSigned *t);
void free_tbs(ToBeSigned *t);
Certificate *copy_certificate(const Certificate *c);
void free_certificate(Certificate *c);
FCPath *copy_fcpath(const FCPath *f);
void free_fcpath(FCPath *f);
PKRoot *copy_pkroot(const PKRoot *p);
void free_pkroot(PKRoot *p);
void free_serial(Serial *s);
void free_privatekeyinfo(PrivateKeyInfo *p);

// ---- DER encoding of SECUDE structures (encode.cpp) -----------------------
Bytes enc_algid(const AlgId *a);
AlgId *dec_algid(const Bytes &der);
Bytes enc_keybits(const KeyBits *k);
KeyBits *dec_keybits(const Bytes &der);
Bytes enc_keyinfo(const KeyInfo *k);
KeyInfo *dec_keyinfo(const Bytes &der);
Bytes enc_dname(const DName *d);
DName *dec_dname(const Bytes &der);
Bytes enc_validity(const Validity *v);
Bytes enc_extensions(const CertExtensions *e);
CertExtensions *dec_extensions(const Bytes &der);
Bytes enc_tbs(const ToBeSigned *t);
Bytes enc_signature_value(const Signature *s);
Bytes enc_certificate(const Certificate *c);
Certificate *dec_certificate(const Bytes &der);
Bytes enc_pkroot(const PKRoot *p);
PKRoot *dec_pkroot(const Bytes &der);
Bytes enc_fcpath(const FCPath *f);
FCPath *dec_fcpath(const Bytes &der);
Bytes enc_privatekeyinfo(const PrivateKeyInfo *p);
PrivateKeyInfo *dec_privatekeyinfo(const Bytes &der);
Bytes enc_unsigned_integer(const uint8_t *p, size_t n);
Bytes strip_leading_zeros(const uint8_t *p, size_t n);
std::string utc_now(long offset_seconds = 0);

// ---- names ------------------------------------------------------------------
std::string dname_to_string(const DName *d);
DName *string_to_dname(const char *s);
bool dname_equal(const DName *a, const DName *b);

// ---- digests (hash.cpp) ----------------------------------------------------
Bytes digest(Hash h, const uint8_t *p, size_t n);
size_t digest_size(Hash h);

// ---- asymmetric (asym.cpp) ------------------------------------------------
struct RsaPublic { Bytes n, e; };
struct RsaPrivate { Bytes n, e, d, p, q; };
struct DsaParams { Bytes p, q, g; };

bool keyinfo_is_rsa(const KeyInfo *k);
bool keyinfo_is_dsa(const KeyInfo *k);
RsaPublic rsa_public_from_keyinfo(const KeyInfo *k);
RsaPrivate rsa_private_from_keyinfo(const KeyInfo *k, const Bytes &e_hint);
int rsa_bits(const Bytes &n);

void rsa_generate(int bits, KeyInfo **pub, KeyInfo **priv);
void dsa_generate(int bits, KeyInfo **pub, KeyInfo **priv);
Bytes rsa_encrypt_blocks(const RsaPublic &k, const uint8_t *p, size_t n);
Bytes rsa_decrypt_blocks(const RsaPrivate &k, const uint8_t *p, size_t n);
Bytes rsa_raw_private(const RsaPrivate &k, const Bytes &m);
Bytes rsa_private_key_der(const RsaPrivate &k);

// Sign/verify the message (hashing included) with the signature algorithm sig_alg.
Bytes sign_message(const KeyInfo *priv, const Bytes &e_hint, const AlgId *sig_alg, const uint8_t *msg, size_t n);
bool verify_message(const KeyInfo *pub, const AlgId *sig_alg, const uint8_t *msg, size_t n, const Bytes &sig);

// ---- symmetric (symcipher.cpp) ----------------------------------------------
Bytes sym_encrypt(const AlgId *alg, const Bytes &key, size_t keybits, const uint8_t *in, size_t n);
Bytes sym_decrypt(const AlgId *alg, const Bytes &key, size_t keybits, const uint8_t *in, size_t n);

// ---- providers (provider.cpp) ---------------------------------------------
OSSL_LIB_CTX *libctx();
bool legacy_enabled();
EVP_MD *fetch_md(const char *name);         // nullptr if unavailable
EVP_CIPHER *fetch_cipher(const char *name); // nullptr if unavailable (or legacy disabled)

// ---- PSE internals (pse.cpp) ----------------------------------------------
struct PseStore;
struct PseObject {
    std::string name;
    Oid type;
    std::string created, updated;
    Bytes value;
};

std::shared_ptr<PseStore> store_of(PSE pse);
std::shared_ptr<PseStore> store_of(PSESel *sel);
const PseObject *store_find(const std::shared_ptr<PseStore> &s, const std::string &name);
void store_put(const std::shared_ptr<PseStore> &s, const std::string &name, const Oid &type, const Bytes &value);
bool store_onekeypaironly(const std::shared_ptr<PseStore> &s);
const char *pse_cadir(PSE pse);
std::string private_key_object(PSE pse, KeyType type);
std::string certificate_object(PSE pse, KeyType type);
KeyInfo *read_private_key(PSE pse, KeyType type);
Bytes rsa_exponent_hint(PSE pse, const KeyInfo *priv);
Certificate *make_ca_certificate(PSE pse, const KeyInfo *pub, const AlgId *sig_alg, const DName *subject,
                                 long validity_seconds);

// ---- print flags (data_flags.cpp) -----------------------------------------
uint32_t print_cert_flags();
uint32_t print_keyinfo_flags();
void *print_cert_flag_address();
void *print_keyinfo_flag_address();

// ---- misc -------------------------------------------------------------------
Bytes random_bytes(size_t n);
Bytes read_file(const std::string &path, bool &exists);
void write_file_atomic(const std::string &path, const Bytes &data, int mode = 0600);
std::string hex(const uint8_t *p, size_t n, bool upper = true);
std::string hexdump(const uint8_t *p, size_t n, const std::string &indent, bool ascii);
std::string format_time(const char *t);
std::string str_printf(const char *fmt, ...);
char *append_string(char *string, const std::string &text);

} // namespace compat

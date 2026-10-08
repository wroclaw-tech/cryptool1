// Private OpenSSL library context with the default provider and, if present,
// the legacy provider (MD4, DES, IDEA, RC2, RC4). The application's default
// context is left untouched. Setting SECUDE_COMPAT_NO_LEGACY_PROVIDER forces
// the built-in implementations.
#include "internal.hpp"

#include <openssl/err.h>
#include <openssl/provider.h>

namespace compat {

namespace {

struct Providers {
    OSSL_LIB_CTX *ctx = nullptr;
    bool legacy = false;

    Providers()
    {
        ctx = OSSL_LIB_CTX_new();
        if (!ctx)
            return;
        ERR_set_mark();
        OSSL_PROVIDER_load(ctx, "default");
        const char *off = std::getenv("SECUDE_COMPAT_NO_LEGACY_PROVIDER");
        if (!(off && *off && std::strcmp(off, "0")))
            legacy = OSSL_PROVIDER_load(ctx, "legacy") != nullptr;
        ERR_pop_to_mark();
    }
};

Providers &providers()
{
    static Providers *p = new Providers;
    return *p;
}

} // namespace

OSSL_LIB_CTX *libctx() { return providers().ctx; }

bool legacy_enabled() { return providers().legacy; }

EVP_MD *fetch_md(const char *name)
{
    ERR_set_mark();
    EVP_MD *md = EVP_MD_fetch(libctx(), name, nullptr);
    ERR_pop_to_mark();
    return md;
}

EVP_CIPHER *fetch_cipher(const char *name)
{
    ERR_set_mark();
    EVP_CIPHER *c = EVP_CIPHER_fetch(libctx(), name, nullptr);
    ERR_pop_to_mark();
    return c;
}

} // namespace compat

extern "C" int secude_compat_legacy_provider_active(void)
{
    return compat::legacy_enabled() ? 1 : 0;
}

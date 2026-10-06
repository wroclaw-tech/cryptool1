#include "legacy.hpp"

namespace compat {

namespace {

const char *evp_name(Hash h)
{
    switch (h) {
    case Hash::MD4: return "MD4";
    case Hash::MD5: return "MD5";
    case Hash::SHA1: return "SHA1";
    case Hash::RIPEMD160: return "RIPEMD160";
    case Hash::SHA256: return "SHA256";
    case Hash::SHA384: return "SHA384";
    case Hash::SHA512: return "SHA512";
    default: return nullptr;
    }
}

class Hasher {
public:
    explicit Hasher(Hash h)
    {
        if (const char *name = evp_name(h)) {
            if (EVP_MD *md = fetch_md(name)) {
                evp_ = EVP_MD_CTX_new();
                bool ok = evp_ && EVP_DigestInit_ex2(evp_, md, nullptr);
                EVP_MD_free(md);
                if (!ok)
                    fail(EINTERNAL, "digest initialisation failed");
                return;
            }
        }
        own_ = legacy::make_digest(h);
        if (!own_)
            fail(EUNKNOWNALGID, "hash algorithm not available");
    }
    ~Hasher() { EVP_MD_CTX_free(evp_); }
    Hasher(const Hasher &) = delete;
    Hasher &operator=(const Hasher &) = delete;

    void update(const uint8_t *p, size_t n)
    {
        if (evp_) {
            if (n && !EVP_DigestUpdate(evp_, p, n))
                fail(EINTERNAL, "digest update failed");
        } else if (n) {
            own_->update(p, n);
        }
    }

    Bytes final()
    {
        if (!evp_)
            return own_->final();
        Bytes out(EVP_MAX_MD_SIZE);
        unsigned int len = 0;
        if (!EVP_DigestFinal_ex(evp_, out.data(), &len))
            fail(EINTERNAL, "digest finalisation failed");
        out.resize(len);
        return out;
    }

private:
    EVP_MD_CTX *evp_ = nullptr;
    std::unique_ptr<legacy::Digest> own_;
};

Hash hash_of(const AlgId *aid)
{
    if (!aid || !aid->objid)
        fail(EALGID, "missing hash algorithm");
    const AlgInfo *a = alg_by_oid(aid->objid);
    if (!a || a->hash == Hash::None)
        fail(EUNKNOWNALGID, "not a hash algorithm: " + oid_to_string(oid_of(aid->objid)));
    return a->hash;
}

} // namespace

Bytes digest(Hash h, const uint8_t *p, size_t n)
{
    Hasher hs(h);
    hs.update(p, n);
    return hs.final();
}

size_t digest_size(Hash h)
{
    switch (h) {
    case Hash::MD2:
    case Hash::MD4:
    case Hash::MD5: return 16;
    case Hash::SHA0:
    case Hash::SHA1:
    case Hash::RIPEMD160: return 20;
    case Hash::SHA256: return 32;
    case Hash::SHA384: return 48;
    case Hash::SHA512: return 64;
    default: return 0;
    }
}

} // namespace compat

using namespace compat;

extern "C" {

RC sec_hash_init(void **context, AlgId *alg_id, HashInput *hash_input)
{
    return guarded<RC>("sec_hash_init", -1, [&] {
        if (!context)
            fail(EINVALID, "missing context");
        *context = new Hasher(hash_of(alg_id));
        return 0;
    });
}

RC sec_hash_more(void **context, OctetString *in_octets)
{
    return guarded<RC>("sec_hash_more", -1, [&] {
        if (!context || !*context)
            fail(ECONTEXTINVALID, "hash context not initialised");
        if (in_octets && in_octets->noctets)
            static_cast<Hasher *>(*context)->update(reinterpret_cast<const uint8_t *>(in_octets->octets), in_octets->noctets);
        return 0;
    });
}

RC sec_hash_end(void **context, OctetString *hash_result)
{
    return guarded<RC>("sec_hash_end", -1, [&] {
        if (!context || !*context)
            fail(ECONTEXTINVALID, "hash context not initialised");
        std::unique_ptr<Hasher> h(static_cast<Hasher *>(*context));
        *context = nullptr;
        Bytes d = h->final();
        if (hash_result)
            fill_ostr(hash_result, d);
        return 0;
    });
}

RC sec_hash_all(OctetString *in_octets, OctetString *hash_result, AlgId *alg_id, HashInput *hash_input)
{
    return guarded<RC>("sec_hash_all", -1, [&] {
        if (!in_octets || !hash_result)
            fail(EINVALID, "missing parameter");
        Bytes d = digest(hash_of(alg_id), reinterpret_cast<const uint8_t *>(in_octets->octets), in_octets->noctets);
        fill_ostr(hash_result, d);
        return 0;
    });
}

} // extern "C"

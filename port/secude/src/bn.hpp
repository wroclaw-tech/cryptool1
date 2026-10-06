#pragma once

#include "internal.hpp"

namespace compat {

struct BnDeleter {
    void operator()(BIGNUM *b) const { BN_clear_free(b); }
};
using Bn = std::unique_ptr<BIGNUM, BnDeleter>;

inline Bn bn_new()
{
    BIGNUM *b = BN_new();
    if (!b)
        throw std::bad_alloc();
    return Bn(b);
}

inline Bn bn_from(const Bytes &be)
{
    Bn b = bn_new();
    if (!be.empty() && !BN_bin2bn(be.data(), int(be.size()), b.get()))
        throw std::bad_alloc();
    return b;
}

inline Bn bn_word(unsigned long w)
{
    Bn b = bn_new();
    if (!BN_set_word(b.get(), w))
        throw std::bad_alloc();
    return b;
}

// Big-endian magnitude, left-padded with zeros to at least `size` octets.
inline Bytes bn_bytes(const BIGNUM *b, size_t size = 0)
{
    size_t n = size_t(BN_num_bytes(b));
    if (n < size)
        n = size;
    Bytes out(n);
    if (n)
        BN_bn2binpad(b, out.data(), int(n));
    return out;
}

inline BN_CTX *bn_ctx()
{
    struct CtxDeleter {
        void operator()(BN_CTX *c) const { BN_CTX_free(c); }
    };
    thread_local std::unique_ptr<BN_CTX, CtxDeleter> c(BN_CTX_secure_new());
    if (!c)
        throw std::bad_alloc();
    return c.get();
}

inline void bn_check(int ok)
{
    if (!ok)
        fail(EINTERNAL, "big number operation failed");
}

} // namespace compat

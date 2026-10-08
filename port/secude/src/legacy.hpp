#pragma once

#include "internal.hpp"

// Built-in implementations of algorithms that OpenSSL 3 lacks (MD2, SHA-0) or
// only offers through the optional legacy provider.
namespace compat {
namespace legacy {

class Digest {
public:
    virtual ~Digest() = default;
    virtual void update(const uint8_t *p, size_t n) = 0;
    virtual Bytes final() = 0;
};

// MD2, MD4, MD5, SHA0, SHA1, RIPEMD160; nullptr for other algorithms.
std::unique_ptr<Digest> make_digest(Hash h);

class BlockCipher {
public:
    virtual ~BlockCipher() = default;
    virtual size_t block_size() const = 0;
    virtual void encrypt(const uint8_t *in, uint8_t *out) const = 0;
    virtual void decrypt(const uint8_t *in, uint8_t *out) const = 0;
};

std::unique_ptr<BlockCipher> make_des(const uint8_t key[8]);
std::unique_ptr<BlockCipher> make_des3(const uint8_t *key, size_t keylen); // 16 or 24 bytes
std::unique_ptr<BlockCipher> make_idea(const uint8_t key[16]);
std::unique_ptr<BlockCipher> make_rc2(const uint8_t *key, size_t keylen, unsigned effective_bits);

void rc4(const uint8_t *key, size_t keylen, const uint8_t *in, uint8_t *out, size_t n);

} // namespace legacy
} // namespace compat

#pragma once

#include "internal.hpp"

namespace compat {

struct CipherSpec {
    Cipher cipher = Cipher::None;
    AlgMode mode = ECB;
    Pad pad = Pad::None;
    Bytes key;
    Bytes iv;
    unsigned rc2_bits = 0;
};

Bytes cipher_encrypt(const CipherSpec &s, const uint8_t *in, size_t n);
Bytes cipher_decrypt(const CipherSpec &s, const uint8_t *in, size_t n);

} // namespace compat

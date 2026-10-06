// Symmetric ciphers. SECUDE "std" padding: the last block is filled with zero
// octets and its final octet holds the number of data octets in that block
// (0..blocksize-1); PKCS padding as in PKCS#5/RFC 1423.
#include "legacy.hpp"
#include "symcipher.hpp"

#include <openssl/core_names.h>

namespace compat {

namespace {

size_t block_size(Cipher c)
{
    switch (c) {
    case Cipher::RC4:
        return 1;
    case Cipher::AES:
        return 16;
    default:
        return 8;
    }
}

void check_key(Cipher c, size_t keylen)
{
    bool ok = false;
    switch (c) {
    case Cipher::DES: ok = keylen == 8; break;
    case Cipher::DES3: ok = keylen == 16 || keylen == 24; break;
    case Cipher::IDEA: ok = keylen == 16; break;
    case Cipher::RC2: ok = keylen >= 1 && keylen <= 128; break;
    case Cipher::RC4: ok = keylen >= 1 && keylen <= 256; break;
    case Cipher::AES: ok = keylen == 16 || keylen == 24 || keylen == 32; break;
    default: break;
    }
    if (!ok)
        fail(EKEYSIZE, str_printf("invalid key length of %u bits", unsigned(keylen * 8)));
}

std::string evp_cipher_name(Cipher c, AlgMode mode, size_t keylen)
{
    bool cbc = mode == CBC;
    switch (c) {
    case Cipher::DES: return cbc ? "DES-CBC" : "DES-ECB";
    case Cipher::DES3:
        if (keylen == 16)
            return cbc ? "DES-EDE-CBC" : "DES-EDE";
        return cbc ? "DES-EDE3-CBC" : "DES-EDE3";
    case Cipher::IDEA: return cbc ? "IDEA-CBC" : "IDEA-ECB";
    case Cipher::RC2: return cbc ? "RC2-CBC" : "RC2-ECB";
    case Cipher::RC4: return "RC4";
    case Cipher::AES: return str_printf("AES-%u-%s", unsigned(keylen * 8), cbc ? "CBC" : "ECB");
    default: return {};
    }
}

bool run_evp(const CipherSpec &s, const uint8_t *in, size_t n, Bytes &out, bool encrypt)
{
    std::string name = evp_cipher_name(s.cipher, s.mode, s.key.size());
    EVP_CIPHER *cipher = name.empty() ? nullptr : fetch_cipher(name.c_str());
    if (!cipher)
        return false;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    bool ok = ctx && EVP_CipherInit_ex2(ctx, cipher, nullptr, nullptr, encrypt, nullptr);
    if (ok && (s.cipher == Cipher::RC2 || s.cipher == Cipher::RC4)) {
        size_t keylen = s.key.size();
        size_t keybits = s.rc2_bits;
        OSSL_PARAM params[3];
        int i = 0;
        params[i++] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_KEYLEN, &keylen);
        if (s.cipher == Cipher::RC2)
            params[i++] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_RC2_KEYBITS, &keybits);
        params[i] = OSSL_PARAM_construct_end();
        ok = EVP_CIPHER_CTX_set_params(ctx, params);
    }
    if (ok)
        ok = EVP_CIPHER_CTX_set_padding(ctx, 0) &&
             EVP_CipherInit_ex2(ctx, nullptr, s.key.data(), s.iv.empty() ? nullptr : s.iv.data(), encrypt, nullptr);
    out.assign(n + 32, 0);
    int len1 = 0, len2 = 0;
    if (ok)
        ok = EVP_CipherUpdate(ctx, out.data(), &len1, in, int(n)) && EVP_CipherFinal_ex(ctx, out.data() + len1, &len2);
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);
    if (!ok)
        fail(EINTERNAL, "cipher operation failed");
    out.resize(size_t(len1 + len2));
    return true;
}

Bytes run_builtin(const CipherSpec &s, const uint8_t *in, size_t n, bool encrypt)
{
    Bytes out(n);
    if (s.cipher == Cipher::RC4) {
        legacy::rc4(s.key.data(), s.key.size(), in, out.data(), n);
        return out;
    }
    std::unique_ptr<legacy::BlockCipher> bc;
    switch (s.cipher) {
    case Cipher::DES: bc = legacy::make_des(s.key.data()); break;
    case Cipher::DES3: bc = legacy::make_des3(s.key.data(), s.key.size()); break;
    case Cipher::IDEA: bc = legacy::make_idea(s.key.data()); break;
    case Cipher::RC2: bc = legacy::make_rc2(s.key.data(), s.key.size(), s.rc2_bits); break;
    default: fail(EUNKNOWNALGID, "cipher not available");
    }
    size_t bs = bc->block_size();
    uint8_t chain[16] = {0};
    if (s.mode == CBC)
        std::memcpy(chain, s.iv.data(), bs);
    for (size_t off = 0; off < n; off += bs) {
        const uint8_t *src = in + off;
        uint8_t *dst = out.data() + off;
        if (s.mode != CBC) {
            encrypt ? bc->encrypt(src, dst) : bc->decrypt(src, dst);
        } else if (encrypt) {
            uint8_t x[16];
            for (size_t i = 0; i < bs; ++i)
                x[i] = src[i] ^ chain[i];
            bc->encrypt(x, dst);
            std::memcpy(chain, dst, bs);
        } else {
            uint8_t saved[16];
            std::memcpy(saved, src, bs);
            bc->decrypt(src, dst);
            for (size_t i = 0; i < bs; ++i)
                dst[i] ^= chain[i];
            std::memcpy(chain, saved, bs);
        }
    }
    return out;
}

Bytes raw(const CipherSpec &s, const uint8_t *in, size_t n, bool encrypt)
{
    Bytes out;
    if (run_evp(s, in, n, out, encrypt))
        return out;
    return run_builtin(s, in, n, encrypt);
}

unsigned rc2_bits_from_version(int version)
{
    switch (version) {
    case 160: return 40;
    case 120: return 64;
    case 58: return 128;
    default: return version >= 256 ? unsigned(version) : 0;
    }
}

CipherSpec spec_from_algid(const AlgId *alg, const Bytes &key)
{
    if (!alg || !alg->objid)
        fail(EALGID, "missing algorithm identifier");
    const AlgInfo *a = alg_by_oid(alg->objid);
    if (!a || a->type != SYM_ENC || a->cipher == Cipher::None)
        fail(EUNKNOWNALGID, "not a symmetric algorithm: " + oid_to_string(oid_of(alg->objid)));
    CipherSpec s;
    s.cipher = a->cipher;
    s.mode = a->mode;
    s.pad = a->pad;
    s.key = key;
    check_key(s.cipher, key.size());
    size_t bs = block_size(s.cipher);
    if (s.mode == CBC) {
        s.iv.assign(bs, 0);
        const OctetString *iv = nullptr;
        if (alg->param && a->parm == PARM_OctetString)
            iv = static_cast<const OctetString *>(alg->param);
        else if (alg->param && a->parm == PARM_RC2CBC)
            iv = &static_cast<const rc2CBC_Parm *>(alg->param)->IV;
        if (iv && iv->noctets) {
            if (iv->noctets != bs)
                fail(EWRONGPARM, "wrong IV length");
            s.iv = bytes_of(iv);
        }
    }
    if (s.cipher == Cipher::RC2) {
        unsigned bits = 0;
        if (alg->param)
            bits = rc2_bits_from_version(static_cast<const rc2CBC_Parm *>(alg->param)->version);
        s.rc2_bits = bits ? bits : unsigned(key.size() * 8);
    }
    return s;
}

} // namespace

Bytes cipher_encrypt(const CipherSpec &s, const uint8_t *in, size_t n)
{
    size_t bs = block_size(s.cipher);
    Bytes data(in, in + n);
    if (bs > 1) {
        if (s.pad == Pad::Std) {
            size_t r = n % bs;
            data.resize(n - r + bs, 0);
            data.back() = uint8_t(r);
        } else if (s.pad == Pad::Pkcs) {
            size_t k = bs - n % bs;
            data.insert(data.end(), k, uint8_t(k));
        } else if (n % bs) {
            fail(EINVALID, "input is not a multiple of the block size");
        }
    }
    return raw(s, data.data(), data.size(), true);
}

Bytes cipher_decrypt(const CipherSpec &s, const uint8_t *in, size_t n)
{
    size_t bs = block_size(s.cipher);
    if (bs > 1 && (n % bs || (s.pad != Pad::None && n == 0)))
        fail(EDECRYPTION, "ciphertext length is not a multiple of the block size");
    Bytes out = raw(s, in, n, false);
    if (bs > 1 && s.pad == Pad::Std) {
        size_t r = out.back();
        if (r >= bs)
            fail(EDECRYPTION, "invalid padding");
        out.resize(out.size() - bs + r);
    } else if (bs > 1 && s.pad == Pad::Pkcs) {
        size_t k = out.back();
        if (k == 0 || k > bs)
            fail(EDECRYPTION, "invalid padding");
        for (size_t i = out.size() - k; i < out.size(); ++i)
            if (out[i] != k)
                fail(EDECRYPTION, "invalid padding");
        out.resize(out.size() - k);
    }
    return out;
}

Bytes sym_encrypt(const AlgId *alg, const Bytes &key, size_t keybits, const uint8_t *in, size_t n)
{
    Bytes k(key.begin(), key.begin() + long(std::min(key.size(), (keybits + 7) / 8)));
    return cipher_encrypt(spec_from_algid(alg, k), in, n);
}

Bytes sym_decrypt(const AlgId *alg, const Bytes &key, size_t keybits, const uint8_t *in, size_t n)
{
    Bytes k(key.begin(), key.begin() + long(std::min(key.size(), (keybits + 7) / 8)));
    return cipher_decrypt(spec_from_algid(alg, k), in, n);
}

} // namespace compat

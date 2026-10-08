// sec_* and af_* encryption, signature and key generation entry points.
//
// RSA encryption (rsa / rsaEncryption keys) is textbook RSA as used by CrypTool:
// the input is split into blocks of (bits-1)/8 octets (the last block is
// padded with zero octets), each block is encrypted into (bits+7)/8 octets.
// Decryption returns (bits-1)/8 octets per block.
#include "internal.hpp"

namespace compat {

namespace {

struct KeyInfoHolder {
    KeyInfo *k = nullptr;
    ~KeyInfoHolder() { free_keyinfo(k); }
};

const KeyInfo *resolve_key(const Key *key, KeyInfoHolder &holder)
{
    if (!key)
        fail(EINVALID, "missing key");
    if (key->key)
        return key->key;
    if (key->pse_sel) {
        auto s = store_of(key->pse_sel);
        std::string name = key->pse_sel->object ? key->pse_sel->object : SKnew_name;
        const PseObject *o = store_find(s, name);
        if (!o)
            fail(EPSEOBJECTNOTEXISTING, "PSE object " + name + " does not exist");
        holder.k = dec_keyinfo(o->value);
        return holder.k;
    }
    fail(EINVALID, "key has neither a value nor a PSE reference");
}

const AlgInfo *algorithm_of(const Key *key, const KeyInfo *k)
{
    const AlgId *a = key->alg && key->alg->objid ? key->alg : k->subjectAI;
    const AlgInfo *info = a ? alg_by_oid(a->objid) : nullptr;
    if (!info)
        fail(EUNKNOWNALGID, "unknown key algorithm");
    return info;
}

const AlgId *cipher_algid(const Key *key, const KeyInfo *k)
{
    return key->alg && key->alg->objid ? key->alg : k->subjectAI;
}

void put_bits(BitString *out, const Bytes &data)
{
    if (!out)
        fail(EINVALID, "missing output");
    if (!out->bits) {
        fill_bstr(out, data, data.size() * 8);
        return;
    }
    size_t off = out->nbits / 8;
    if (!data.empty())
        std::memcpy(out->bits + off, data.data(), data.size());
    out->nbits = sec_uint4((off + data.size()) * 8);
}

void put_octets(OctetString *out, const Bytes &data)
{
    if (!out)
        fail(EINVALID, "missing output");
    if (!out->octets) {
        fill_ostr(out, data);
        return;
    }
    size_t off = out->noctets;
    if (!data.empty())
        std::memcpy(out->octets + off, data.data(), data.size());
    out->noctets = sec_uint4(off + data.size());
}

Bytes encrypt(const Key *key, const uint8_t *in, size_t n)
{
    KeyInfoHolder holder;
    const KeyInfo *k = resolve_key(key, holder);
    const AlgInfo *a = algorithm_of(key, k);
    if (a->enc == SECUDE_ALG_RSA)
        return rsa_encrypt_blocks(rsa_public_from_keyinfo(k), in, n);
    if (a->type != SYM_ENC)
        fail(EALGNOTFITKEY, std::string(a->name) + " cannot be used for encryption");
    return sym_encrypt(cipher_algid(key, k), bytes_of(&k->subjectkey), k->subjectkey.nbits, in, n);
}

Bytes decrypt(const Key *key, const Bytes &e_hint, const uint8_t *in, size_t n)
{
    KeyInfoHolder holder;
    const KeyInfo *k = resolve_key(key, holder);
    const AlgInfo *a = algorithm_of(key, k);
    if (a->enc == SECUDE_ALG_RSA)
        return rsa_decrypt_blocks(rsa_private_from_keyinfo(k, e_hint), in, n);
    if (a->type != SYM_ENC)
        fail(EALGNOTFITKEY, std::string(a->name) + " cannot be used for decryption");
    return sym_decrypt(cipher_algid(key, k), bytes_of(&k->subjectkey), k->subjectkey.nbits, in, n);
}

const uint8_t *data_of(const OctetString *o) { return o ? reinterpret_cast<const uint8_t *>(o->octets) : nullptr; }

int key_bits(const KeyInfo *k)
{
    if (keyinfo_is_rsa(k)) {
        KeyBits *kb = dec_keybits(bytes_of(&k->subjectkey));
        Bytes a = bytes_of(&kb->part1), b = bytes_of(&kb->part2);
        bool pub = kb->choice == 2 && a.size() > b.size() + 4;
        free_keybits_content(kb);
        mem_free(kb);
        if (pub)
            return rsa_bits(a);
        return rsa_bits(a) + rsa_bits(b);
    }
    if (keyinfo_is_dsa(k) && k->subjectAI->param)
        return rsa_bits(bytes_of(&static_cast<const KeyBits *>(k->subjectAI->param)->part1));
    return int(k->subjectkey.nbits);
}

} // namespace

} // namespace compat

using namespace compat;

extern "C" {

int sec_encrypt_all(OctetString *in_octets, BitString *out_bits, Key *key)
{
    return guarded<int>("sec_encrypt_all", -1, [&] {
        if (!in_octets)
            fail(EINVALID, "missing input");
        put_bits(out_bits, encrypt(key, data_of(in_octets), in_octets->noctets));
        return 0;
    });
}

int sec_decrypt_all(BitString *in_bits, OctetString *out_octets, Key *key)
{
    return guarded<int>("sec_decrypt_all", -1, [&] {
        if (!in_bits)
            fail(EINVALID, "missing input");
        Bytes in = bytes_of(in_bits);
        put_octets(out_octets, decrypt(key, Bytes(), in.data(), in.size()));
        return 0;
    });
}

RC sec_verify_all(OctetString *in_octets, Signature *signature, Key *key, HashInput *hash_input)
{
    return guarded<RC>("sec_verify_all", -1, [&] {
        if (!in_octets || !signature)
            fail(EINVALID, "missing parameter");
        KeyInfoHolder holder;
        const KeyInfo *k = resolve_key(key, holder);
        if (!verify_message(k, signature->signAI, data_of(in_octets), in_octets->noctets, bytes_of(&signature->signature)))
            fail(EVERIFY, "signature verification failed");
        return 0;
    });
}

RC rsa_sign_all(OctetString *hash, BitString *sign, KeyBits *key)
{
    return guarded<RC>("rsa_sign_all", -1, [&] {
        if (!hash || !sign || !key)
            fail(EINVALID, "missing parameter");
        KeyInfoHolder holder;
        holder.k = mem_new<KeyInfo>();
        holder.k->subjectAI = copy_algid(&rsa_aid);
        Bytes kb = enc_keybits(key);
        fill_bstr(&holder.k->subjectkey, kb, kb.size() * 8);
        Bytes sig = rsa_raw_private(rsa_private_from_keyinfo(holder.k, Bytes()), bytes_of(hash));
        fill_bstr(sign, sig, sig.size() * 8);
        return 0;
    });
}

int sec_get_key(KeyInfo *keyinfo, Key *key)
{
    return guarded<int>("sec_get_key", -1, [&] {
        if (!keyinfo)
            fail(EINVALID, "missing output");
        KeyInfoHolder holder;
        KeyInfo *copy = copy_keyinfo(resolve_key(key, holder));
        *keyinfo = *copy;
        mem_free(copy);
        return 0;
    });
}

OctetString *sec_random_ostr(sec_uint4 noctects, int security)
{
    return guarded<OctetString *>("sec_random_ostr", nullptr, [&] { return new_ostr(random_bytes(noctects)); });
}

int af_encrypt_all(PSE pse_handle, OctetString *inoctets, BitString *outbits, Key *key, DName *dname)
{
    return guarded<int>("af_encrypt_all", -1, [&] {
        if (!inoctets)
            fail(EINVALID, "missing input");
        store_of(pse_handle);
        if (key && (key->key || key->pse_sel)) {
            put_bits(outbits, encrypt(key, data_of(inoctets), inoctets->noctets));
            return 0;
        }
        Certificate *own = af_pse_get_Certificate(pse_handle, ENCRYPTION, nullptr, nullptr);
        if (!own)
            fail(EPSEOBJECTNOTEXISTING, "no encryption key available");
        Key k{};
        k.key = own->tbs->subjectPK;
        try {
            put_bits(outbits, encrypt(&k, data_of(inoctets), inoctets->noctets));
        } catch (...) {
            free_certificate(own);
            throw;
        }
        free_certificate(own);
        return 0;
    });
}

int af_decrypt_all(PSE pse_handle, BitString *inbits, OctetString *outoctets, Key *key)
{
    return guarded<int>("af_decrypt_all", -1, [&] {
        if (!inbits)
            fail(EINVALID, "missing input");
        Bytes in = bytes_of(inbits);
        if (key && (key->key || key->pse_sel)) {
            KeyInfoHolder holder;
            Bytes hint = rsa_exponent_hint(pse_handle, resolve_key(key, holder));
            put_octets(outoctets, decrypt(key, hint, in.data(), in.size()));
            return 0;
        }
        KeyInfoHolder priv;
        priv.k = read_private_key(pse_handle, ENCRYPTION);
        Key k{};
        k.key = priv.k;
        put_octets(outoctets, decrypt(&k, rsa_exponent_hint(pse_handle, priv.k), in.data(), in.size()));
        return 0;
    });
}

RC af_sign_all(PSE pse_handle, OctetString *in_octets, Signature *signature)
{
    return guarded<RC>("af_sign_all", -1, [&] {
        if (!in_octets || !signature)
            fail(EINVALID, "missing parameter");
        KeyInfoHolder priv;
        priv.k = read_private_key(pse_handle, SIGNATURE);
        if (!signature->signAI)
            signature->signAI = keyinfo_is_dsa(priv.k) ? &dsaWithSHA1_aid : &sha1WithRSASignature_aid;
        Bytes sig = sign_message(priv.k, rsa_exponent_hint(pse_handle, priv.k), signature->signAI, data_of(in_octets),
                                 in_octets->noctets);
        fill_bstr(&signature->signature, sig, sig.size() * 8);
        return 0;
    });
}

RC af_gen_key(PSE pse_handle, Key *key, KeyType ktype, Boolean replace)
{
    return guarded<RC>("af_gen_key", -1, [&] {
        auto store = store_of(pse_handle);
        if (!key || !key->alg || !key->alg->objid)
            fail(EINVALID, "missing key algorithm");
        const AlgInfo *a = alg_by_oid(key->alg->objid);
        if (!a || (a->enc != SECUDE_ALG_RSA && a->enc != SECUDE_ALG_DSA))
            fail(EUNKNOWNALGID, "key generation supports RSA and DSA only");
        int bits = key->key_size > 0 ? key->key_size : DEF_ASYM_KEYSIZE;
        std::string object = store_onekeypaironly(store) ? SKnew_name : (ktype == SIGNATURE ? SignSK_name : DecSKnew_name);
        if (!replace && store_find(store, object))
            fail(EPSEALREADYEXISTING, "PSE object " + object + " already exists");
        KeyInfo *pub = nullptr, *priv = nullptr;
        if (a->enc == SECUDE_ALG_RSA)
            rsa_generate(bits, &pub, &priv);
        else
            dsa_generate(bits, &pub, &priv);
        KeyInfoHolder priv_holder;
        priv_holder.k = priv;
        try {
            store_put(store, object, oid_of(pse_object_oid(object.c_str())), enc_keyinfo(priv));
        } catch (...) {
            free_keyinfo(pub);
            throw;
        }
        key->key = pub;
        return 0;
    });
}

RC af_pse_get_keysize(PSE pse_handle, int *sig, int *enc, AlgEnc *sig_type)
{
    return guarded<RC>("af_pse_get_keysize", -1, [&] {
        KeyInfoHolder s, e;
        s.k = read_private_key(pse_handle, SIGNATURE);
        e.k = read_private_key(pse_handle, ENCRYPTION);
        if (sig)
            *sig = key_bits(s.k);
        if (enc)
            *enc = key_bits(e.k);
        if (sig_type)
            *sig_type = keyinfo_is_dsa(s.k) ? SECUDE_ALG_DSA : SECUDE_ALG_RSA;
        return 0;
    });
}

} // extern "C"

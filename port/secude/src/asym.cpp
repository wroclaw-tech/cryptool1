// RSA and DSA with SECUDE key representations:
//   RSA public  KeyInfo: rsa (2.5.8.1.1, INTEGER keysize), KeyBits {n, e}
//   RSA private KeyInfo: rsa, KeyBits {p, q} (or {-, -, n, d}); e from the
//                        PSE certificate, 65537 otherwise
//   DSA public/private:  dsa (1.3.14.3.2.12, KeyBits {p, q, g}), INTEGER y / x
#include "bn.hpp"
#include "der.hpp"

namespace compat {

namespace {

const unsigned long kF4 = 65537;

KeyBits *keybits_of(const KeyInfo *k)
{
    if (!k)
        fail(EINVALID, "missing key");
    return dec_keybits(bytes_of(&k->subjectkey));
}

struct KeyBitsHolder {
    KeyBits *k;
    explicit KeyBitsHolder(KeyBits *kb) : k(kb) {}
    ~KeyBitsHolder()
    {
        free_keybits_content(k);
        mem_free(k);
    }
    KeyBitsHolder(const KeyBitsHolder &) = delete;
    KeyBitsHolder &operator=(const KeyBitsHolder &) = delete;
};

AlgEnc enc_of(const AlgId *a)
{
    const AlgInfo *i = a ? alg_by_oid(a->objid) : nullptr;
    return i ? i->enc : NoAlgEnc;
}

AlgId *rsa_algid(int bits)
{
    AlgId *a = mem_new<AlgId>();
    a->objid = copy_objid(rsa_aid.objid);
    unsigned int *p = mem_new<unsigned int>();
    *p = unsigned(bits);
    a->param = p;
    return a;
}

KeyInfo *keyinfo_from_parts(AlgId *alg, std::initializer_list<const Bytes *> parts)
{
    KeyBits kb{};
    OctetString *dst[] = {&kb.part1, &kb.part2, &kb.part3, &kb.part4, &kb.part5};
    int i = 0;
    for (const Bytes *p : parts)
        fill_ostr(dst[i++], *p);
    kb.choice = i;
    Bytes der_bytes;
    try {
        der_bytes = enc_keybits(&kb);
    } catch (...) {
        free_keybits_content(&kb);
        throw;
    }
    free_keybits_content(&kb);
    KeyInfo *k = mem_new<KeyInfo>();
    k->subjectAI = alg;
    fill_bstr(&k->subjectkey, der_bytes, der_bytes.size() * 8);
    return k;
}

Bn generate_prime(int bits, const BIGNUM *add = nullptr, const BIGNUM *rem = nullptr)
{
    Bn p = bn_new();
    bn_check(BN_generate_prime_ex2(p.get(), bits, 0, add, rem, nullptr, bn_ctx()));
    return p;
}

Bytes digest_info(Hash h, const Bytes &hash)
{
    static const struct {
        Hash h;
        const Oid *oid;
    } map[] = {{Hash::MD2, &oids::md2},       {Hash::MD4, &oids::md4},     {Hash::MD5, &oids::md5},
               {Hash::SHA0, &oids::sha},      {Hash::SHA1, &oids::sha1},   {Hash::RIPEMD160, &oids::ripemd160},
               {Hash::SHA256, &oids::sha256}, {Hash::SHA384, &oids::sha384}, {Hash::SHA512, &oids::sha512}};
    for (const auto &m : map)
        if (m.h == h)
            return der::seq({der::seq({der::oid(*m.oid), der::null()}), der::octet_string(hash)});
    fail(EUNKNOWNALGID, "no DigestInfo for hash");
}

Bytes pkcs1_type1(const Bytes &t, size_t k)
{
    if (t.size() + 11 > k)
        fail(EKEYSIZE, "RSA key too short for this signature algorithm");
    Bytes em(k, 0xff);
    em[0] = 0x00;
    em[1] = 0x01;
    em[k - t.size() - 1] = 0x00;
    std::memcpy(em.data() + k - t.size(), t.data(), t.size());
    return em;
}

struct DsaKey {
    Bn p, q, g, key;
};

DsaKey dsa_key(const KeyInfo *k)
{
    if (!k || !k->subjectAI || !k->subjectAI->param)
        fail(EALGNOTFITKEY, "DSA key without domain parameters");
    auto *params = static_cast<const KeyBits *>(k->subjectAI->param);
    Bytes key_der = bytes_of(&k->subjectkey);
    der::Node n = der::parse_one(key_der);
    DsaKey d{bn_from(bytes_of(&params->part1)), bn_from(bytes_of(&params->part2)), bn_from(bytes_of(&params->part3)),
             bn_from(der::get_unsigned(n))};
    if (BN_is_zero(d.q.get()) || BN_is_zero(d.p.get()))
        fail(EALGNOTFITKEY, "invalid DSA domain parameters");
    return d;
}

Bn dsa_message(const DsaKey &k, Hash h, const uint8_t *msg, size_t n)
{
    Bytes hv = digest(h, msg, n);
    Bn z = bn_from(hv);
    int qbits = BN_num_bits(k.q.get());
    int hbits = int(hv.size() * 8);
    if (hbits > qbits)
        bn_check(BN_rshift(z.get(), z.get(), hbits - qbits));
    return z;
}

Bn mod_exp(const BIGNUM *b, const BIGNUM *e, const BIGNUM *m)
{
    Bn r = bn_new();
    bn_check(BN_mod_exp(r.get(), b, e, m, bn_ctx()));
    return r;
}

Bn rsa_private_op(const RsaPrivate &k, const BIGNUM *c)
{
    Bn n = bn_from(k.n);
    if (BN_cmp(c, n.get()) >= 0)
        fail(EDECRYPTION, "ciphertext block out of range");
    if (k.p.empty() || k.q.empty())
        return mod_exp(c, bn_from(k.d).get(), n.get());
    Bn p = bn_from(k.p), q = bn_from(k.q), d = bn_from(k.d);
    Bn one = bn_word(1), pm1 = bn_new(), qm1 = bn_new(), dp = bn_new(), dq = bn_new(), qinv = bn_new();
    bn_check(BN_sub(pm1.get(), p.get(), one.get()));
    bn_check(BN_sub(qm1.get(), q.get(), one.get()));
    bn_check(BN_mod(dp.get(), d.get(), pm1.get(), bn_ctx()));
    bn_check(BN_mod(dq.get(), d.get(), qm1.get(), bn_ctx()));
    if (!BN_mod_inverse(qinv.get(), q.get(), p.get(), bn_ctx()))
        fail(EALGNOTFITKEY, "invalid RSA primes");
    Bn m1 = bn_new(), m2 = bn_new(), h = bn_new(), r = bn_new();
    bn_check(BN_mod_exp_mont_consttime(m1.get(), c, dp.get(), p.get(), bn_ctx(), nullptr));
    bn_check(BN_mod_exp_mont_consttime(m2.get(), c, dq.get(), q.get(), bn_ctx(), nullptr));
    bn_check(BN_mod_sub(h.get(), m1.get(), m2.get(), p.get(), bn_ctx()));
    bn_check(BN_mod_mul(h.get(), h.get(), qinv.get(), p.get(), bn_ctx()));
    bn_check(BN_mul(r.get(), h.get(), q.get(), bn_ctx()));
    bn_check(BN_add(r.get(), r.get(), m2.get()));
    return r;
}

} // namespace

bool keyinfo_is_rsa(const KeyInfo *k) { return k && enc_of(k->subjectAI) == SECUDE_ALG_RSA; }

bool keyinfo_is_dsa(const KeyInfo *k) { return k && enc_of(k->subjectAI) == SECUDE_ALG_DSA; }

int rsa_bits(const Bytes &n) { return BN_num_bits(bn_from(n).get()); }

RsaPublic rsa_public_from_keyinfo(const KeyInfo *k)
{
    if (!keyinfo_is_rsa(k))
        fail(EALGNOTFITKEY, "not an RSA key");
    KeyBitsHolder kb(keybits_of(k));
    if (!kb.k->part1.noctets || !kb.k->part2.noctets)
        fail(EALGNOTFITKEY, "incomplete RSA public key");
    return {bytes_of(&kb.k->part1), bytes_of(&kb.k->part2)};
}

RsaPrivate rsa_private_from_keyinfo(const KeyInfo *k, const Bytes &e_hint)
{
    if (!keyinfo_is_rsa(k))
        fail(EALGNOTFITKEY, "not an RSA key");
    KeyBitsHolder kb(keybits_of(k));
    RsaPrivate r;
    Bn e = e_hint.empty() ? bn_word(kF4) : bn_from(e_hint);
    r.e = bn_bytes(e.get());
    if (kb.k->part1.noctets && kb.k->part2.noctets) {
        r.p = bytes_of(&kb.k->part1);
        r.q = bytes_of(&kb.k->part2);
        Bn p = bn_from(r.p), q = bn_from(r.q), n = bn_new(), one = bn_word(1);
        Bn pm1 = bn_new(), qm1 = bn_new(), phi = bn_new(), d = bn_new();
        bn_check(BN_mul(n.get(), p.get(), q.get(), bn_ctx()));
        bn_check(BN_sub(pm1.get(), p.get(), one.get()));
        bn_check(BN_sub(qm1.get(), q.get(), one.get()));
        bn_check(BN_mul(phi.get(), pm1.get(), qm1.get(), bn_ctx()));
        if (!BN_mod_inverse(d.get(), e.get(), phi.get(), bn_ctx()))
            fail(EALGNOTFITKEY, "public exponent does not fit the RSA primes");
        r.n = bn_bytes(n.get());
        r.d = bn_bytes(d.get());
    } else if (kb.k->part3.noctets && kb.k->part4.noctets) {
        r.n = bytes_of(&kb.k->part3);
        r.d = bytes_of(&kb.k->part4);
    } else {
        fail(EALGNOTFITKEY, "incomplete RSA private key");
    }
    return r;
}

void rsa_generate(int bits, KeyInfo **pub, KeyInfo **priv)
{
    if (bits < MIN_ASYM_KEYSIZE || bits > MAXKEYLENGTH)
        fail(EKEYSIZE, str_printf("unsupported RSA key size %d", bits));
    Bn e = bn_word(kF4), one = bn_word(1);
    int pbits = (bits + 1) / 2, qbits = bits - pbits;
    for (;;) {
        Bn p = generate_prime(pbits), q = generate_prime(qbits);
        Bn pm1 = bn_new(), qm1 = bn_new(), g1 = bn_new(), g2 = bn_new(), n = bn_new();
        bn_check(BN_sub(pm1.get(), p.get(), one.get()));
        bn_check(BN_sub(qm1.get(), q.get(), one.get()));
        bn_check(BN_gcd(g1.get(), pm1.get(), e.get(), bn_ctx()));
        bn_check(BN_gcd(g2.get(), qm1.get(), e.get(), bn_ctx()));
        bn_check(BN_mul(n.get(), p.get(), q.get(), bn_ctx()));
        if (!BN_is_one(g1.get()) || !BN_is_one(g2.get()) || !BN_cmp(p.get(), q.get()) || BN_num_bits(n.get()) != bits)
            continue;
        Bytes nb = bn_bytes(n.get()), eb = bn_bytes(e.get()), pb = bn_bytes(p.get()), qb = bn_bytes(q.get());
        *pub = keyinfo_from_parts(rsa_algid(bits), {&nb, &eb});
        try {
            *priv = keyinfo_from_parts(rsa_algid(bits), {&pb, &qb});
        } catch (...) {
            free_keyinfo(*pub);
            *pub = nullptr;
            throw;
        }
        return;
    }
}

void dsa_generate(int bits, KeyInfo **pub, KeyInfo **priv)
{
    if (bits < 320 || bits > MAXKEYLENGTH)
        fail(EKEYSIZE, str_printf("unsupported DSA key size %d", bits));
    Bn q = generate_prime(160);
    Bn add = bn_new(), rem = bn_word(1);
    bn_check(BN_lshift1(add.get(), q.get()));
    Bn p = generate_prime(bits, add.get(), rem.get());
    Bn pm1 = bn_new(), e = bn_new(), g = bn_new(), one = bn_word(1);
    bn_check(BN_sub(pm1.get(), p.get(), one.get()));
    bn_check(BN_div(e.get(), nullptr, pm1.get(), q.get(), bn_ctx()));
    for (unsigned long h = 2;; ++h) {
        g = mod_exp(bn_word(h).get(), e.get(), p.get());
        if (!BN_is_one(g.get()))
            break;
    }
    Bn x = bn_new(), y;
    do {
        bn_check(BN_priv_rand_range(x.get(), q.get()));
    } while (BN_is_zero(x.get()));
    y = mod_exp(g.get(), x.get(), p.get());

    auto make = [&](const BIGNUM *value) {
        KeyBits *params = mem_new<KeyBits>();
        fill_ostr(&params->part1, bn_bytes(p.get()));
        fill_ostr(&params->part2, bn_bytes(q.get()));
        fill_ostr(&params->part3, bn_bytes(g.get()));
        params->choice = 3;
        KeyInfo *k = mem_new<KeyInfo>();
        k->subjectAI = mem_new<AlgId>();
        k->subjectAI->objid = copy_objid(dsa_aid.objid);
        k->subjectAI->param = params;
        Bytes v = der::integer_unsigned(bn_bytes(value));
        fill_bstr(&k->subjectkey, v, v.size() * 8);
        return k;
    };
    *pub = make(y.get());
    *priv = make(x.get());
}

Bytes rsa_encrypt_blocks(const RsaPublic &k, const uint8_t *p, size_t n)
{
    int bits = rsa_bits(k.n);
    size_t in_block = size_t(bits - 1) / 8, out_block = size_t(bits + 7) / 8;
    if (in_block == 0)
        fail(EKEYSIZE, "RSA modulus too small");
    Bn modulus = bn_from(k.n), e = bn_from(k.e);
    Bytes out;
    for (size_t off = 0; off < n; off += in_block) {
        Bytes block(in_block, 0);
        std::memcpy(block.data(), p + off, std::min(in_block, n - off));
        Bn c = mod_exp(bn_from(block).get(), e.get(), modulus.get());
        der::append(out, bn_bytes(c.get(), out_block));
    }
    return out;
}

Bytes rsa_decrypt_blocks(const RsaPrivate &k, const uint8_t *p, size_t n)
{
    int bits = rsa_bits(k.n);
    size_t out_block = size_t(bits - 1) / 8, in_block = size_t(bits + 7) / 8;
    if (n % in_block)
        fail(EDECRYPTION, "ciphertext length does not match the RSA modulus");
    Bytes out;
    for (size_t off = 0; off < n; off += in_block) {
        Bn m = rsa_private_op(k, bn_from(Bytes(p + off, p + off + in_block)).get());
        der::append(out, bn_bytes(m.get(), out_block));
    }
    return out;
}

Bytes rsa_raw_private(const RsaPrivate &k, const Bytes &m)
{
    size_t size = size_t(rsa_bits(k.n) + 7) / 8;
    return bn_bytes(rsa_private_op(k, bn_from(m).get()).get(), size);
}

Bytes rsa_private_key_der(const RsaPrivate &k)
{
    if (k.p.empty() || k.q.empty())
        fail(EALGNOTFITKEY, "RSA private key without primes");
    Bn p = bn_from(k.p), q = bn_from(k.q), d = bn_from(k.d), one = bn_word(1);
    Bn pm1 = bn_new(), qm1 = bn_new(), dp = bn_new(), dq = bn_new(), qinv = bn_new();
    bn_check(BN_sub(pm1.get(), p.get(), one.get()));
    bn_check(BN_sub(qm1.get(), q.get(), one.get()));
    bn_check(BN_mod(dp.get(), d.get(), pm1.get(), bn_ctx()));
    bn_check(BN_mod(dq.get(), d.get(), qm1.get(), bn_ctx()));
    if (!BN_mod_inverse(qinv.get(), q.get(), p.get(), bn_ctx()))
        fail(EALGNOTFITKEY, "invalid RSA primes");
    return der::seq({der::integer(0), der::integer_unsigned(k.n), der::integer_unsigned(k.e), der::integer_unsigned(k.d),
                     der::integer_unsigned(k.p), der::integer_unsigned(k.q), der::integer_unsigned(bn_bytes(dp.get())),
                     der::integer_unsigned(bn_bytes(dq.get())), der::integer_unsigned(bn_bytes(qinv.get()))});
}

Bytes sign_message(const KeyInfo *priv, const Bytes &e_hint, const AlgId *sig_alg, const uint8_t *msg, size_t n)
{
    const AlgInfo *a = sig_alg ? alg_by_oid(sig_alg->objid) : nullptr;
    if (!a || a->type != SIG || a->hash == Hash::None)
        fail(EUNKNOWNALGID, "not a signature algorithm");
    if (a->enc == SECUDE_ALG_RSA) {
        if (!keyinfo_is_rsa(priv))
            fail(EALGNOTFITKEY, "RSA signature algorithm needs an RSA key");
        RsaPrivate k = rsa_private_from_keyinfo(priv, e_hint);
        size_t size = size_t(rsa_bits(k.n) + 7) / 8;
        return rsa_raw_private(k, pkcs1_type1(digest_info(a->hash, digest(a->hash, msg, n)), size));
    }
    if (!keyinfo_is_dsa(priv))
        fail(EALGNOTFITKEY, "DSA signature algorithm needs a DSA key");
    DsaKey k = dsa_key(priv);
    Bn z = dsa_message(k, a->hash, msg, n);
    for (;;) {
        Bn nonce = bn_new(), kinv = bn_new(), r = bn_new(), s = bn_new();
        bn_check(BN_priv_rand_range(nonce.get(), k.q.get()));
        if (BN_is_zero(nonce.get()))
            continue;
        Bn gk = mod_exp(k.g.get(), nonce.get(), k.p.get());
        bn_check(BN_nnmod(r.get(), gk.get(), k.q.get(), bn_ctx()));
        if (BN_is_zero(r.get()) || !BN_mod_inverse(kinv.get(), nonce.get(), k.q.get(), bn_ctx()))
            continue;
        bn_check(BN_mod_mul(s.get(), k.key.get(), r.get(), k.q.get(), bn_ctx()));
        bn_check(BN_mod_add(s.get(), s.get(), z.get(), k.q.get(), bn_ctx()));
        bn_check(BN_mod_mul(s.get(), s.get(), kinv.get(), k.q.get(), bn_ctx()));
        if (BN_is_zero(s.get()))
            continue;
        return der::seq({der::integer_unsigned(bn_bytes(r.get())), der::integer_unsigned(bn_bytes(s.get()))});
    }
}

bool verify_message(const KeyInfo *pub, const AlgId *sig_alg, const uint8_t *msg, size_t n, const Bytes &sig)
{
    const AlgInfo *a = sig_alg ? alg_by_oid(sig_alg->objid) : nullptr;
    if (!a || a->type != SIG || a->hash == Hash::None)
        fail(EUNKNOWNALGID, "not a signature algorithm");
    if (a->enc == SECUDE_ALG_RSA) {
        if (!keyinfo_is_rsa(pub))
            fail(EALGNOTFITKEY, "RSA signature algorithm needs an RSA key");
        RsaPublic k = rsa_public_from_keyinfo(pub);
        Bn modulus = bn_from(k.n), s = bn_from(sig);
        if (BN_cmp(s.get(), modulus.get()) >= 0)
            return false;
        size_t size = size_t(rsa_bits(k.n) + 7) / 8;
        Bytes em = bn_bytes(mod_exp(s.get(), bn_from(k.e).get(), modulus.get()).get(), size);
        return em == pkcs1_type1(digest_info(a->hash, digest(a->hash, msg, n)), size);
    }
    if (!keyinfo_is_dsa(pub))
        fail(EALGNOTFITKEY, "DSA signature algorithm needs a DSA key");
    DsaKey k = dsa_key(pub);
    der::Node node = der::parse_one(sig);
    if (node.tag != der::SEQUENCE)
        return false;
    der::Reader rd(node);
    Bn r = bn_from(der::get_unsigned(rd.next(der::INTEGER))), s = bn_from(der::get_unsigned(rd.next(der::INTEGER)));
    rd.expect_end();
    if (BN_is_zero(r.get()) || BN_is_zero(s.get()) || BN_cmp(r.get(), k.q.get()) >= 0 || BN_cmp(s.get(), k.q.get()) >= 0)
        return false;
    Bn z = dsa_message(k, a->hash, msg, n);
    Bn w = bn_new(), u1 = bn_new(), u2 = bn_new(), v = bn_new();
    if (!BN_mod_inverse(w.get(), s.get(), k.q.get(), bn_ctx()))
        return false;
    bn_check(BN_mod_mul(u1.get(), z.get(), w.get(), k.q.get(), bn_ctx()));
    bn_check(BN_mod_mul(u2.get(), r.get(), w.get(), k.q.get(), bn_ctx()));
    bn_check(BN_mod_exp2_mont(v.get(), k.g.get(), u1.get(), k.key.get(), u2.get(), k.p.get(), bn_ctx(), nullptr));
    bn_check(BN_nnmod(v.get(), v.get(), k.q.get(), bn_ctx()));
    return BN_cmp(v.get(), r.get()) == 0;
}

} // namespace compat

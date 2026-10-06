// PKCS#12 (RFC 7292) encoding and decoding of P12_Safe structures and
// PKCS#8 PrivateKeyInfo creation. Supports the password-integrity mode with
// pkcs-12PbeIds (3DES, RC2, RC4) and PBES2 (PBKDF2 with AES/3DES).
#include "bn.hpp"
#include "der.hpp"
#include "symcipher.hpp"

#include <algorithm>

namespace compat {

namespace {

constexpr int kDefaultIterations = 2048;

struct PbeInfo {
    const Oid *oid;
    Cipher cipher;
    size_t keylen;
    unsigned rc2_bits;
};

const PbeInfo kPbe[] = {
    {&oids::pbeSHA1RC4_128, Cipher::RC4, 16, 0},  {&oids::pbeSHA1RC4_40, Cipher::RC4, 5, 0},
    {&oids::pbeSHA1DES3, Cipher::DES3, 24, 0},    {&oids::pbeSHA1DES2, Cipher::DES3, 16, 0},
    {&oids::pbeSHA1RC2_128, Cipher::RC2, 16, 128}, {&oids::pbeSHA1RC2_40, Cipher::RC2, 5, 40},
};

const PbeInfo *pbe_by_oid(const Oid &o)
{
    for (const auto &p : kPbe)
        if (*p.oid == o)
            return &p;
    return nullptr;
}

struct HashChoice {
    Hash hash;
    const Oid *digest_oid;
    const Oid *hmac_oid;
    const char *name;
    size_t block;
};

const HashChoice kHashes[] = {
    {Hash::SHA1, &oids::sha1, &oids::hmacSHA1, "SHA1", 64},
    {Hash::SHA256, &oids::sha256, &oids::hmacSHA256, "SHA256", 64},
    {Hash::SHA384, &oids::sha384, &oids::hmacSHA384, "SHA384", 128},
    {Hash::SHA512, &oids::sha512, &oids::hmacSHA512, "SHA512", 128},
};

const HashChoice &hash_by_digest_oid(const Oid &o)
{
    for (const auto &h : kHashes)
        if (*h.digest_oid == o)
            return h;
    fail(EP12INTMODE, "unsupported PKCS#12 MAC algorithm " + oid_to_string(o));
}

const HashChoice &hash_by_hmac_oid(const Oid &o)
{
    for (const auto &h : kHashes)
        if (*h.hmac_oid == o)
            return h;
    fail(EP12ENCALG, "unsupported PBKDF2 PRF " + oid_to_string(o));
}

// RFC 7292 appendix B.2
Bytes p12_kdf(const HashChoice &h, const Bytes &password, const Bytes &salt, int iterations, uint8_t id, size_t n)
{
    size_t u = digest_size(h.hash), v = h.block;
    auto stretch = [v](const Bytes &in) {
        Bytes out;
        if (in.empty())
            return out;
        size_t len = v * ((in.size() + v - 1) / v);
        for (size_t i = 0; i < len; ++i)
            out.push_back(in[i % in.size()]);
        return out;
    };
    Bytes d(v, id), i = stretch(salt), p = stretch(password);
    der::append(i, p);
    Bytes out;
    while (out.size() < n) {
        Bytes a = d;
        der::append(a, i);
        a = digest(h.hash, a.data(), a.size());
        for (int r = 1; r < iterations; ++r)
            a = digest(h.hash, a.data(), a.size());
        out.insert(out.end(), a.begin(), a.begin() + long(std::min(u, n - out.size())));
        Bytes b;
        while (b.size() < v)
            der::append(b, a);
        b.resize(v);
        for (size_t off = 0; off < i.size(); off += v) {
            unsigned carry = 1;
            for (size_t k = v; k-- > 0;) {
                carry += unsigned(i[off + k]) + b[k];
                i[off + k] = uint8_t(carry);
                carry >>= 8;
            }
        }
    }
    return out;
}

Bytes hmac(const HashChoice &h, const Bytes &key, const Bytes &data)
{
    Bytes out(EVP_MAX_MD_SIZE);
    size_t len = 0;
    if (!EVP_Q_mac(libctx(), "HMAC", nullptr, h.name, nullptr, key.data(), key.size(), data.data(), data.size(),
                   out.data(), out.size(), &len))
        fail(EINTERNAL, "HMAC failed");
    out.resize(len);
    return out;
}

// BMPString password (as produced by aux_latin1_to_unicode) as Latin-1 text for PBKDF2.
std::string password_text(const Bytes &bmp)
{
    std::string s;
    for (size_t i = 0; i + 1 < bmp.size(); i += 2) {
        unsigned c = (unsigned(bmp[i]) << 8) | bmp[i + 1];
        if (!c)
            break;
        s += char(c < 0x100 ? c : '?');
    }
    return s;
}

Bytes utf8(const std::string &latin1)
{
    Bytes t = der::string(der::UTF8String, latin1);
    return der::parse_one(t).value();
}

Bytes pbe_crypt(const Bytes &algid_der, const Bytes &password, const uint8_t *in, size_t n, bool encrypt)
{
    der::Reader r(der::parse_one(algid_der));
    Oid oid = der::get_oid(r.next());
    der::Node params = r.next(der::SEQUENCE);
    CipherSpec s;
    if (const PbeInfo *p = pbe_by_oid(oid)) {
        der::Reader pr(params);
        Bytes salt = pr.next(der::OCTET_STRING).value();
        int iter = int(der::get_integer(pr.next(der::INTEGER)));
        const HashChoice &h = kHashes[0];
        s.cipher = p->cipher;
        s.key = p12_kdf(h, password, salt, iter, 1, p->keylen);
        if (p->cipher != Cipher::RC4) {
            s.mode = CBC;
            s.pad = Pad::Pkcs;
            s.iv = p12_kdf(h, password, salt, iter, 2, 8);
        }
        s.rc2_bits = p->rc2_bits;
    } else if (oid == oids::pbes2) {
        der::Reader pr(params);
        der::Reader kdf(pr.next(der::SEQUENCE));
        der::Reader enc(pr.next(der::SEQUENCE));
        if (der::get_oid(kdf.next()) != oids::pbkdf2)
            fail(EP12ENCALG, "unsupported PBES2 key derivation");
        der::Reader kp(kdf.next(der::SEQUENCE));
        Bytes salt = kp.next(der::OCTET_STRING).value();
        int iter = int(der::get_integer(kp.next(der::INTEGER)));
        der::Node node;
        long long keylen = 0;
        if (kp.next_optional(der::INTEGER, node))
            keylen = der::get_integer(node);
        const HashChoice *prf = &kHashes[0];
        if (kp.next_optional(der::SEQUENCE, node))
            prf = &hash_by_hmac_oid(der::get_oid(der::Reader(node).next()));
        Oid eoid = der::get_oid(enc.next());
        s.iv = enc.next(der::OCTET_STRING).value();
        s.mode = CBC;
        s.pad = Pad::Pkcs;
        size_t klen;
        if (eoid == oids::aes128CBC || eoid == oids::aes192CBC || eoid == oids::aes256CBC) {
            s.cipher = Cipher::AES;
            klen = eoid == oids::aes128CBC ? 16 : eoid == oids::aes192CBC ? 24 : 32;
        } else if (eoid == oids::desEDE3CBC) {
            s.cipher = Cipher::DES3;
            klen = 24;
        } else {
            fail(EP12ENCALG, "unsupported PBES2 cipher " + oid_to_string(eoid));
        }
        if (keylen && size_t(keylen) != klen)
            fail(EP12ENCALG, "inconsistent PBES2 key length");
        std::string pw = password_text(password);
        Bytes pw8 = utf8(pw);
        EVP_MD *md = fetch_md(prf->name);
        s.key.assign(klen, 0);
        bool ok = md && PKCS5_PBKDF2_HMAC(reinterpret_cast<const char *>(pw8.data()), int(pw8.size()), salt.data(),
                                          int(salt.size()), iter, md, int(klen), s.key.data());
        EVP_MD_free(md);
        if (!ok)
            fail(EINTERNAL, "PBKDF2 failed");
    } else {
        fail(EP12ENCALG, "unsupported PKCS#12 encryption algorithm " + oid_to_string(oid));
    }
    return encrypt ? cipher_encrypt(s, in, n) : cipher_decrypt(s, in, n);
}

Bytes pbe_algid(const Oid &oid, int iterations)
{
    if (!pbe_by_oid(oid))
        fail(EP12ENCALG, "unsupported PKCS#12 encryption algorithm " + oid_to_string(oid));
    return der::seq({der::oid(oid), der::seq({der::octet_string(random_bytes(8)), der::integer(iterations)})});
}

Bytes attributes(const P12_Bag *b)
{
    std::vector<Bytes> attrs;
    if (b->friendlyName && b->friendlyName->noctets)
        attrs.push_back(der::seq({der::oid(oids::friendlyName),
                                  der::set_of({der::tlv(der::BMPString, bytes_of(b->friendlyName))})}));
    if (b->localKeyID && b->localKeyID->noctets)
        attrs.push_back(der::seq({der::oid(oids::localKeyId), der::set_of({der::octet_string(bytes_of(b->localKeyID))})}));
    if (attrs.empty())
        return {};
    return der::set_of(attrs);
}

Bytes safe_bag(const Oid &type, const Bytes &value, const P12_Bag *b)
{
    Bytes body = der::concat({der::oid(type), der::tlv(der::ctx(0), value)});
    der::append(body, attributes(b));
    return der::tlv(der::SEQUENCE, body);
}

Bytes content_info_data(const Bytes &content)
{
    return der::seq({der::oid(oids::data), der::tlv(der::ctx(0), der::octet_string(content))});
}

Bytes content_info_encrypted(const Bytes &content, const Oid &alg, int iterations, const Bytes &password)
{
    Bytes algid = pbe_algid(alg, iterations);
    Bytes enc = pbe_crypt(algid, password, content.data(), content.size(), true);
    Bytes eci = der::seq({der::oid(oids::data), algid, der::tlv(der::ctx(0, false), enc)});
    return der::seq({der::oid(oids::encryptedData), der::tlv(der::ctx(0), der::seq({der::integer(0), eci}))});
}

Bytes octet_content(const der::Node &n)
{
    if (n.tag == der::OCTET_STRING)
        return n.value();
    if (n.tag == (der::OCTET_STRING | 0x20)) {
        Bytes out;
        der::Reader r(n);
        while (!r.empty())
            der::append(out, octet_content(r.next()));
        return out;
    }
    fail(EDECODE, "OCTET STRING expected");
}

void append_bag(P12_Safe *safe, P12_Bag *bag)
{
    P12_Bag **tail = &safe->bags;
    while (*tail)
        tail = &(*tail)->next;
    *tail = bag;
}

void parse_attributes(const der::Node &set, P12_Bag *bag)
{
    der::Reader r(set);
    while (!r.empty()) {
        der::Reader a(r.next(der::SEQUENCE));
        Oid id = der::get_oid(a.next());
        der::Reader values(a.next(der::SET));
        if (values.empty())
            continue;
        der::Node v = values.next();
        if (id == oids::friendlyName && v.tag == der::BMPString && !bag->friendlyName)
            bag->friendlyName = new_ostr(v.value());
        else if (id == oids::localKeyId && v.tag == der::OCTET_STRING && !bag->localKeyID)
            bag->localKeyID = new_ostr(v.value());
    }
}

void keep_lowest(P12_Safe *safe, int iterations)
{
    if (!safe->iterationCount || iterations < safe->iterationCount)
        safe->iterationCount = iterations;
}

int iterations_of(const Bytes &algid_der)
{
    der::Reader r(der::parse_one(algid_der));
    Oid oid = der::get_oid(r.next());
    der::Reader p(r.next(der::SEQUENCE));
    if (oid == oids::pbes2) {
        der::Reader kdf(p.next(der::SEQUENCE));
        kdf.next();
        der::Reader kp(kdf.next(der::SEQUENCE));
        kp.next();
        return int(der::get_integer(kp.next(der::INTEGER)));
    }
    p.next();
    return int(der::get_integer(p.next(der::INTEGER)));
}

void parse_safe_contents(const Bytes &der_bytes, const Bytes &password, P12_Safe *safe, bool encrypted)
{
    der::Reader bags(der::parse_one(der_bytes));
    while (!bags.empty()) {
        der::Reader b(bags.next(der::SEQUENCE));
        Oid type = der::get_oid(b.next());
        der::Node value = der::Reader(b.next(der::ctx(0))).next();
        der::Node attrs;
        bool have_attrs = b.next_optional(der::SET, attrs);
        P12_Bag *bag = nullptr;
        if (type == oids::keyBag || type == oids::pkcs8ShroudedKeyBag) {
            Bytes pkcs8 = value.raw();
            if (type == oids::pkcs8ShroudedKeyBag) {
                der::Reader e(value);
                Bytes algid = e.next(der::SEQUENCE).raw();
                Bytes enc = e.next(der::OCTET_STRING).value();
                pkcs8 = pbe_crypt(algid, password, enc.data(), enc.size(), false);
                if (!safe->alg_oid.espvk)
                    safe->alg_oid.espvk = new_objid(der::get_oid(der::Reader(der::parse_one(algid)).next()));
                keep_lowest(safe, iterations_of(algid));
            }
            bag = mem_new<P12_Bag>();
            bag->type = P12_bc_key;
            bag->int_encryption = type == oids::pkcs8ShroudedKeyBag;
            bag->content.key = dec_privatekeyinfo(pkcs8);
        } else if (type == oids::certBag) {
            der::Reader c(value);
            if (der::get_oid(c.next()) != oids::x509Certificate)
                continue;
            Bytes cert = octet_content(der::Reader(c.next(der::ctx(0))).next());
            bag = mem_new<P12_Bag>();
            bag->type = P12_bc_cert;
            bag->int_encryption = encrypted;
            bag->content.cert = dec_certificate(cert);
        } else {
            continue;
        }
        append_bag(safe, bag);
        if (have_attrs)
            parse_attributes(attrs, bag);
    }
}

} // namespace

} // namespace compat

using namespace compat;

extern "C" {

int pkcs12_encode(P12_Safe *safe, OctetString *password, OctetString *asn1_out)
{
    return guarded<int>("pkcs12_encode", -1, [&] {
        if (!safe || !password || !asn1_out)
            fail(EINVALID, "missing parameter");
        Bytes pw = bytes_of(password);
        int iterations = safe->iterationCount > 0 ? safe->iterationCount : kDefaultIterations;
        Oid enc_alg = safe->alg_oid.enc ? oid_of(safe->alg_oid.enc) : oids::pbeSHA1RC2_40;
        Oid key_alg = safe->alg_oid.espvk ? oid_of(safe->alg_oid.espvk) : oids::pbeSHA1DES3;
        Bytes certs_encrypted, certs_plain, keys;
        for (const P12_Bag *b = safe->bags; b; b = b->next) {
            if (b->type == P12_bc_cert && b->content.cert) {
                Bytes cert = enc_certificate(b->content.cert);
                Bytes bag = safe_bag(oids::certBag,
                                     der::seq({der::oid(oids::x509Certificate), der::tlv(der::ctx(0), der::octet_string(cert))}), b);
                der::append(b->int_encryption ? certs_encrypted : certs_plain, bag);
            } else if (b->type == P12_bc_key && b->content.key) {
                Bytes pkcs8 = enc_privatekeyinfo(b->content.key);
                if (b->int_encryption) {
                    Bytes algid = pbe_algid(key_alg, iterations);
                    Bytes enc = pbe_crypt(algid, pw, pkcs8.data(), pkcs8.size(), true);
                    der::append(keys, safe_bag(oids::pkcs8ShroudedKeyBag, der::seq({algid, der::octet_string(enc)}), b));
                } else {
                    der::append(keys, safe_bag(oids::keyBag, pkcs8, b));
                }
            }
        }
        Bytes auth;
        if (!certs_encrypted.empty())
            der::append(auth, content_info_encrypted(der::tlv(der::SEQUENCE, certs_encrypted), enc_alg, iterations, pw));
        if (!certs_plain.empty())
            der::append(auth, content_info_data(der::tlv(der::SEQUENCE, certs_plain)));
        if (!keys.empty())
            der::append(auth, content_info_data(der::tlv(der::SEQUENCE, keys)));
        Bytes auth_safe = der::tlv(der::SEQUENCE, auth);

        const HashChoice &h = hash_by_digest_oid(safe->alg_oid.mac ? oid_of(safe->alg_oid.mac) : oids::sha1);
        Bytes salt = random_bytes(8);
        Bytes mac = hmac(h, p12_kdf(h, pw, salt, iterations, 3, digest_size(h.hash)), auth_safe);
        Bytes mac_data = der::seq({der::seq({der::seq({der::oid(*h.digest_oid), der::null()}), der::octet_string(mac)}),
                                   der::octet_string(salt), der::integer(iterations)});
        fill_ostr(asn1_out, der::seq({der::integer(3), content_info_data(auth_safe), mac_data}));
        return 0;
    });
}

int pkcs12_decode(OctetString *asn1string, OctetString *password, P12_Safe *safe, Boolean *verified)
{
    return guarded<int>("pkcs12_decode", -1, [&] {
        if (!asn1string || !password || !safe)
            fail(EINVALID, "missing parameter");
        Bytes pw = bytes_of(password);
        Bytes in = bytes_of(asn1string);
        der::Reader pfx(der::parse_one(in));
        if (der::get_integer(pfx.next(der::INTEGER)) != 3)
            fail(EDECODE, "unsupported PFX version");
        der::Reader ci(pfx.next(der::SEQUENCE));
        if (der::get_oid(ci.next()) != oids::data)
            fail(EP12INTMODE, "only password integrity mode is supported");
        Bytes auth_safe = octet_content(der::Reader(ci.next(der::ctx(0))).next());

        if (verified)
            *verified = FALSE;
        der::Node mac_node;
        if (pfx.next_optional(der::SEQUENCE, mac_node)) {
            der::Reader md(mac_node);
            der::Reader di(md.next(der::SEQUENCE));
            Oid digest_oid = der::get_oid(der::Reader(di.next(der::SEQUENCE)).next());
            Bytes expected = di.next(der::OCTET_STRING).value();
            Bytes salt = md.next(der::OCTET_STRING).value();
            der::Node it;
            int iterations = md.next_optional(der::INTEGER, it) ? int(der::get_integer(it)) : 1;
            const HashChoice &h = hash_by_digest_oid(digest_oid);
            Bytes mac = hmac(h, p12_kdf(h, pw, salt, iterations, 3, digest_size(h.hash)), auth_safe);
            if (mac != expected)
                fail(EVERIFY, "PKCS#12 integrity check failed (wrong password?)");
            if (verified)
                *verified = TRUE;
            safe->alg_oid.mac = new_objid(digest_oid);
            keep_lowest(safe, iterations);
        }

        safe->version = 3;
        der::Reader cis(der::parse_one(auth_safe));
        while (!cis.empty()) {
            der::Reader c(cis.next(der::SEQUENCE));
            Oid type = der::get_oid(c.next());
            der::Node content = der::Reader(c.next(der::ctx(0))).next();
            if (type == oids::data) {
                parse_safe_contents(octet_content(content), pw, safe, false);
            } else if (type == oids::encryptedData) {
                der::Reader ed(content);
                ed.next(der::INTEGER);
                der::Reader eci(ed.next(der::SEQUENCE));
                eci.next();
                Bytes algid = eci.next(der::SEQUENCE).raw();
                der::Node enc = eci.next();
                Bytes ct = enc.tag == der::ctx(0, false) ? enc.value() : octet_content(der::Reader(enc).next());
                Bytes plain = pbe_crypt(algid, pw, ct.data(), ct.size(), false);
                if (!safe->alg_oid.enc)
                    safe->alg_oid.enc = new_objid(der::get_oid(der::Reader(der::parse_one(algid)).next()));
                keep_lowest(safe, iterations_of(algid));
                parse_safe_contents(plain, pw, safe, true);
            }
        }
        return 0;
    });
}

PrivateKeyInfo *aux_create_PrivateKeyInfo(KeyInfo *key, Certificate *cert, SET_OF_Attr *attributes)
{
    return guarded<PrivateKeyInfo *>("aux_create_PrivateKeyInfo", nullptr, [&] {
        if (!key)
            fail(EINVALID, "missing private key");
        if (attributes)
            fail(ENOTSUPPORTED, "PKCS#8 attributes are not supported");
        PrivateKeyInfo *p = mem_new<PrivateKeyInfo>();
        try {
            if (keyinfo_is_rsa(key)) {
                Bytes e;
                if (cert && cert->tbs && keyinfo_is_rsa(cert->tbs->subjectPK))
                    e = rsa_public_from_keyinfo(cert->tbs->subjectPK).e;
                RsaPrivate k = rsa_private_from_keyinfo(key, e);
                p->privateKeyAlgorithm = copy_algid(&rsaEncryption_aid);
                p->privateKey = new_ostr(rsa_private_key_der(k));
            } else if (keyinfo_is_dsa(key)) {
                if (!key->subjectAI->param)
                    fail(EALGNOTFITKEY, "DSA key without domain parameters");
                p->privateKeyAlgorithm = mem_new<AlgId>();
                p->privateKeyAlgorithm->objid = new_objid(oids::id_dsa);
                p->privateKeyAlgorithm->param = copy_keybits(static_cast<const KeyBits *>(key->subjectAI->param));
                p->privateKey = new_ostr(bytes_of(&key->subjectkey));
            } else {
                fail(EALGNOTFITKEY, "only RSA and DSA keys can be exported");
            }
            return p;
        } catch (...) {
            free_privatekeyinfo(p);
            throw;
        }
    });
}

} // extern "C"

#include "bn.hpp"
#include "der.hpp"
#include "testing.hpp"

#include <openssl/core_names.h>
#include <openssl/param_build.h>

using namespace compat;

namespace {

struct KeyPair {
    KeyInfo *pub = nullptr, *priv = nullptr;
    ~KeyPair()
    {
        aux_free_KeyInfo(&pub);
        aux_free_KeyInfo(&priv);
    }
};

Signature sign(KeyInfo *priv, AlgId *alg, const Bytes &msg)
{
    Bytes s = sign_message(priv, Bytes(), alg, msg.data(), msg.size());
    Signature sig{};
    sig.signAI = alg;
    fill_bstr(&sig.signature, s, s.size() * 8);
    return sig;
}

int verify(KeyInfo *pub, Signature *sig, const Bytes &msg)
{
    Key key{};
    key.key = pub;
    OctetString in{sec_uint4(msg.size()), reinterpret_cast<char *>(const_cast<uint8_t *>(msg.data()))};
    return sec_verify_all(&in, sig, &key, nullptr);
}

EVP_PKEY *openssl_rsa(const RsaPublic &k)
{
    Bn n = bn_from(k.n), e = bn_from(k.e);
    OSSL_PARAM_BLD *b = OSSL_PARAM_BLD_new();
    OSSL_PARAM_BLD_push_BN(b, OSSL_PKEY_PARAM_RSA_N, n.get());
    OSSL_PARAM_BLD_push_BN(b, OSSL_PKEY_PARAM_RSA_E, e.get());
    OSSL_PARAM *params = OSSL_PARAM_BLD_to_param(b);
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr);
    EVP_PKEY *pkey = nullptr;
    EVP_PKEY_fromdata_init(ctx);
    EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params);
    EVP_PKEY_CTX_free(ctx);
    OSSL_PARAM_free(params);
    OSSL_PARAM_BLD_free(b);
    return pkey;
}

} // namespace

TEST(asym_rsa_sign_verify_all_algorithms)
{
    for (int bits : {512, 768}) {
        KeyPair kp;
        rsa_generate(bits, &kp.pub, &kp.priv);
        RsaPublic pub = rsa_public_from_keyinfo(kp.pub);
        CHECK_EQ(rsa_bits(pub.n), bits);
        CHECK_HEX(pub.e.data(), pub.e.size(), "010001");
        Bytes msg = random_bytes(100);
        for (AlgId *alg : {&md2WithRsaEncryption_aid, &md5WithRsaEncryption_aid, &shaWithRSASignature_aid,
                           &sha1WithRSASignature_aid, &ripemd160WithRSASignature_aid, &sha1WithRsaEncryption_aid}) {
            Signature sig = sign(kp.priv, alg, msg);
            CHECK_EQ(sig.signature.nbits, sec_uint4(bits));
            CHECK_EQ(verify(kp.pub, &sig, msg), 0);
            sig.signature.bits[3] ^= 1;
            CHECK_EQ(verify(kp.pub, &sig, msg), -1);
            aux_free2_BitString(&sig.signature);
        }
        // PKCS#1 v1.5 interoperability
        Signature sig = sign(kp.priv, &md5WithRsaEncryption_aid, msg);
        EVP_PKEY *pkey = openssl_rsa(pub);
        EVP_MD_CTX *mctx = EVP_MD_CTX_new();
        CHECK_EQ(EVP_DigestVerifyInit_ex(mctx, nullptr, "MD5", nullptr, nullptr, pkey, nullptr), 1);
        CHECK_EQ(EVP_DigestVerify(mctx, reinterpret_cast<uint8_t *>(sig.signature.bits), sig.signature.nbits / 8,
                                  msg.data(), msg.size()),
                 1);
        EVP_MD_CTX_free(mctx);
        EVP_PKEY_free(pkey);
        aux_free2_BitString(&sig.signature);
    }
    aux_free_error();
}

TEST(asym_rsa_small_key_and_blocks)
{
    KeyPair kp;
    rsa_generate(301, &kp.pub, &kp.priv);
    RsaPublic pub = rsa_public_from_keyinfo(kp.pub);
    CHECK_EQ(rsa_bits(pub.n), 301);
    RsaPrivate priv = rsa_private_from_keyinfo(kp.priv, Bytes());
    // block sizes: 37 octets of plain text per 38 octet cipher block
    Bytes msg = random_bytes(37 * 3);
    Bytes c = rsa_encrypt_blocks(pub, msg.data(), msg.size());
    CHECK_EQ(c.size(), size_t(38 * 3));
    CHECK(rsa_decrypt_blocks(priv, c.data(), c.size()) == msg);
    Bytes partial = random_bytes(40);
    c = rsa_encrypt_blocks(pub, partial.data(), partial.size());
    Bytes d = rsa_decrypt_blocks(priv, c.data(), c.size());
    CHECK_EQ(d.size(), size_t(74));
    CHECK(Bytes(d.begin(), d.begin() + 40) == partial);
    CHECK(Bytes(d.begin() + 40, d.end()) == Bytes(34, 0));
}

TEST(asym_rsa_textbook_property_for_side_channel_demo)
{
    KeyPair kp;
    rsa_generate(512, &kp.pub, &kp.priv);
    RsaPublic pub = rsa_public_from_keyinfo(kp.pub);
    RsaPrivate priv = rsa_private_from_keyinfo(kp.priv, Bytes());
    Bytes session = random_bytes(16);
    Bytes block(63 - 16, 0);
    block.insert(block.end(), session.begin(), session.end());
    Bytes c = rsa_encrypt_blocks(pub, block.data(), block.size());
    CHECK_EQ(c.size(), size_t(64));
    // C' = C * 3^e mod n decrypts to 3*m mod n
    Bn n = bn_from(pub.n), e = bn_from(pub.e), three = bn_word(3), f = bn_new(), c2 = bn_new();
    BN_CTX *ctx = BN_CTX_new();
    BN_mod_exp(f.get(), three.get(), e.get(), n.get(), ctx);
    BN_mod_mul(c2.get(), bn_from(c).get(), f.get(), n.get(), ctx);
    Bytes m2 = rsa_decrypt_blocks(priv, bn_bytes(c2.get(), 64).data(), 64);
    Bn expect = bn_new();
    BN_mod_mul(expect.get(), bn_from(block).get(), three.get(), n.get(), ctx);
    CHECK(BN_cmp(bn_from(m2).get(), expect.get()) == 0);
    BN_CTX_free(ctx);
}

TEST(asym_rsa_user_chosen_exponent)
{
    Bytes e17{0x11};
    KeyPair kp;
    RsaPrivate k17;
    for (;;) {
        aux_free_KeyInfo(&kp.pub);
        aux_free_KeyInfo(&kp.priv);
        rsa_generate(512, &kp.pub, &kp.priv);
        try {
            k17 = rsa_private_from_keyinfo(kp.priv, e17);
            break;
        } catch (const Error &e) {
            CHECK_EQ(e.code, EALGNOTFITKEY); // 17 divides (p-1)(q-1)
        }
    }
    RsaPrivate base = rsa_private_from_keyinfo(kp.priv, Bytes());
    CHECK(k17.d != base.d);
    Bytes m = random_bytes(20);
    Bytes s = rsa_raw_private(k17, m);
    Bn r = bn_new();
    BN_CTX *ctx = BN_CTX_new();
    BN_mod_exp(r.get(), bn_from(s).get(), bn_from(e17).get(), bn_from(k17.n).get(), ctx);
    CHECK(BN_cmp(r.get(), bn_from(m).get()) == 0);
    BN_CTX_free(ctx);
}

TEST(asym_dsa_sign_verify)
{
    KeyPair kp;
    dsa_generate(512, &kp.pub, &kp.priv);
    CHECK(keyinfo_is_dsa(kp.pub));
    auto *params = static_cast<KeyBits *>(kp.pub->subjectAI->param);
    CHECK_EQ(rsa_bits(bytes_of(&params->part1)), 512);
    CHECK_EQ(rsa_bits(bytes_of(&params->part2)), 160);
    Bytes msg = random_bytes(77);
    for (AlgId *alg : {&dsaWithSHA_aid, &dsaWithSHA1_aid}) {
        Signature sig = sign(kp.priv, alg, msg);
        CHECK_EQ(verify(kp.pub, &sig, msg), 0);
        Bytes other = msg;
        other[0] ^= 1;
        CHECK_EQ(verify(kp.pub, &sig, other), -1);
        aux_free2_BitString(&sig.signature);
    }
    Signature wrong = sign(kp.priv, &dsaWithSHA1_aid, msg);
    wrong.signAI = &md5WithRsaEncryption_aid;
    CHECK_EQ(verify(kp.pub, &wrong, msg), -1);
    CHECK_EQ(th_last_error(), EALGNOTFITKEY);
    aux_free2_BitString(&wrong.signature);
    aux_free_error();
}

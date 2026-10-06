#include "legacy.hpp"
#include "testing.hpp"

#include <openssl/core_names.h>
#include <openssl/provider.h>

using namespace compat;

namespace {

Bytes unhex(const std::string &h)
{
    Bytes b;
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        b.push_back(uint8_t(std::stoi(h.substr(i, 2), nullptr, 16)));
    return b;
}

struct SymKey {
    Key key{};
    KeyInfo info{};
    Bytes material;
    SymKey(AlgId *alg, const Bytes &k) : material(k)
    {
        info.subjectAI = alg;
        info.subjectkey.bits = reinterpret_cast<char *>(material.data());
        info.subjectkey.nbits = sec_uint4(material.size() * 8);
        key.key = &info;
    }
};

Bytes encrypt(AlgId *alg, const Bytes &key, const Bytes &plain)
{
    SymKey k(alg, key);
    OctetString in{sec_uint4(plain.size()), reinterpret_cast<char *>(const_cast<uint8_t *>(plain.data()))};
    std::vector<char> buf(plain.size() + 64);
    BitString out{0, buf.data()};
    CHECK_EQ(sec_encrypt_all(&in, &out, &k.key), 0);
    return Bytes(buf.begin(), buf.begin() + out.nbits / 8);
}

int decrypt(AlgId *alg, const Bytes &key, const Bytes &cipher, Bytes &plain)
{
    SymKey k(alg, key);
    BitString in{sec_uint4(cipher.size() * 8), reinterpret_cast<char *>(const_cast<uint8_t *>(cipher.data()))};
    std::vector<char> buf(cipher.size() + 64);
    OctetString out{0, buf.data()};
    int rc = sec_decrypt_all(&in, &out, &k.key);
    plain.assign(buf.begin(), buf.begin() + out.noctets);
    return rc;
}

} // namespace

TEST(cipher_des_known_answer)
{
    Bytes key = unhex("133457799bbcdff1"), pt = unhex("0123456789abcdef");
    uint8_t ct[8];
    legacy::make_des(key.data())->encrypt(pt.data(), ct);
    CHECK_HEX(ct, 8, "85e813540f0ab405");
    Bytes c = encrypt(&desECB_aid, key, pt);
    CHECK_EQ(c.size(), size_t(16));
    CHECK_HEX(c.data(), 8, "85e813540f0ab405");
    Bytes back;
    CHECK_EQ(decrypt(&desECB_aid, key, c, back), 0);
    CHECK(back == pt);
}

TEST(cipher_idea_known_answer)
{
    Bytes key = unhex("00010002000300040005000600070008"), pt = unhex("0000000100020003");
    auto idea = legacy::make_idea(key.data());
    uint8_t ct[8], back[8];
    idea->encrypt(pt.data(), ct);
    CHECK_HEX(ct, 8, "11fbed2b01986de5");
    idea->decrypt(ct, back);
    CHECK_HEX(back, 8, "0000000100020003");
    Bytes c = encrypt(&idea_aid, key, pt);
    CHECK_HEX(c.data(), 8, "11fbed2b01986de5");
}

TEST(cipher_rc2_rfc2268)
{
    struct {
        const char *key;
        unsigned bits;
        const char *pt;
        const char *ct;
    } v[] = {
        {"0000000000000000", 63, "0000000000000000", "ebb773f993278eff"},
        {"ffffffffffffffff", 64, "ffffffffffffffff", "278b27e42e2f0d49"},
        {"3000000000000000", 64, "1000000000000001", "30649edf9be7d2c2"},
        {"88", 64, "0000000000000000", "61a8a244adacccf0"},
        {"88bca90e90875a", 64, "0000000000000000", "6ccf4308974c267f"},
        {"88bca90e90875a7f0f79c384627bafb2", 64, "0000000000000000", "1a807d272bbe5db1"},
        {"88bca90e90875a7f0f79c384627bafb2", 128, "0000000000000000", "2269552ab0f85ca6"},
    };
    for (const auto &t : v) {
        Bytes key = unhex(t.key), pt = unhex(t.pt);
        auto rc2 = legacy::make_rc2(key.data(), key.size(), t.bits);
        uint8_t ct[8], back[8];
        rc2->encrypt(pt.data(), ct);
        CHECK_HEX(ct, 8, t.ct);
        rc2->decrypt(ct, back);
        CHECK_HEX(back, 8, t.pt);
    }
}

TEST(cipher_rc4_known_answer)
{
    const char *pt = "Plaintext";
    uint8_t out[9];
    legacy::rc4(reinterpret_cast<const uint8_t *>("Key"), 3, reinterpret_cast<const uint8_t *>(pt), out, 9);
    CHECK_HEX(out, 9, "bbf316e8d940af0ad3");
    Bytes c = encrypt(&rc4_aid, Bytes{'K', 'e', 'y'}, Bytes(pt, pt + 9));
    CHECK_HEX(c.data(), c.size(), "bbf316e8d940af0ad3");
}

TEST(cipher_round_trips_and_padding)
{
    struct {
        AlgId *alg;
        size_t keylen;
        size_t block;
        bool std_pad;
    } algs[] = {
        {&desECB_aid, 8, 8, true},  {&desCBC_pad_aid, 8, 8, false}, {&desCBC3_aid, 16, 8, true},
        {&desEDE_aid, 16, 8, true}, {&idea_aid, 16, 8, true},        {&rc2CBC_aid, 1, 8, false},
        {&rc2CBC_aid, 16, 8, false}, {&rc4_aid, 5, 1, false},        {&rc4_aid, 16, 1, false},
        {&aes128CBC_aid, 16, 16, false}, {&desCBC3_aid, 24, 8, true},
    };
    for (const auto &a : algs) {
        Bytes key = random_bytes(a.keylen);
        for (size_t len = 0; len < 35; ++len) {
            Bytes pt = random_bytes(len);
            Bytes c = encrypt(a.alg, key, pt);
            size_t expected = a.block == 1 ? len : (len / a.block + 1) * a.block;
            CHECK_EQ(c.size(), expected);
            Bytes back;
            CHECK_EQ(decrypt(a.alg, key, c, back), 0);
            CHECK(back == pt);
        }
    }
}

TEST(cipher_std_padding_layout)
{
    Bytes key = unhex("0123456789abcdef");
    Bytes pt = {'a', 'b', 'c'};
    Bytes c = encrypt(&desECB_aid, key, pt);
    uint8_t block[8];
    legacy::make_des(key.data())->decrypt(c.data(), block);
    CHECK_HEX(block, 8, "6162630000000003");
}

TEST(cipher_bad_padding_is_decryption_error)
{
    Bytes key = unhex("0123456789abcdef");
    uint8_t garbage[8] = {0x61, 0x62, 0x63, 0, 0, 0, 0, 0x09};
    uint8_t ct[8];
    legacy::make_des(key.data())->encrypt(garbage, ct);
    Bytes back;
    CHECK_EQ(decrypt(&desECB_aid, key, Bytes(ct, ct + 8), back), -1);
    ErrStack *err = th_remove_last_error();
    CHECK_EQ(err->e_number, 1792);
    aux_free(err->e_text);
    aux_free(err->e_proc);
    aux_free(err);
    CHECK_EQ(decrypt(&desECB_aid, key, Bytes(ct, ct + 7), back), -1);
    aux_free_error();
}

TEST(cipher_builtin_matches_openssl_legacy)
{
    OSSL_LIB_CTX *ctx = OSSL_LIB_CTX_new();
    OSSL_PROVIDER_load(ctx, "default");
    if (!OSSL_PROVIDER_load(ctx, "legacy")) {
        std::printf("     (legacy provider not available, cross-check skipped)\n");
        OSSL_LIB_CTX_free(ctx);
        return;
    }
    auto ref = [&](const char *name, const Bytes &key, unsigned rc2bits, const Bytes &in) {
        EVP_CIPHER *c = EVP_CIPHER_fetch(ctx, name, nullptr);
        EVP_CIPHER_CTX *cc = EVP_CIPHER_CTX_new();
        EVP_CipherInit_ex2(cc, c, nullptr, nullptr, 1, nullptr);
        size_t keylen = key.size(), bits = rc2bits;
        if (EVP_CIPHER_get_key_length(c) != int(keylen)) {
            OSSL_PARAM p[3] = {OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_KEYLEN, &keylen),
                               OSSL_PARAM_construct_end(), OSSL_PARAM_construct_end()};
            if (rc2bits)
                p[1] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_RC2_KEYBITS, &bits);
            EVP_CIPHER_CTX_set_params(cc, p);
        } else if (rc2bits) {
            OSSL_PARAM p[2] = {OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_RC2_KEYBITS, &bits), OSSL_PARAM_construct_end()};
            EVP_CIPHER_CTX_set_params(cc, p);
        }
        EVP_CIPHER_CTX_set_padding(cc, 0);
        EVP_CipherInit_ex2(cc, nullptr, key.data(), nullptr, 1, nullptr);
        Bytes out(in.size() + 16);
        int n = 0, f = 0;
        EVP_CipherUpdate(cc, out.data(), &n, in.data(), int(in.size()));
        EVP_CipherFinal_ex(cc, out.data() + n, &f);
        out.resize(size_t(n + f));
        EVP_CIPHER_CTX_free(cc);
        EVP_CIPHER_free(c);
        return out;
    };
    auto ecb = [](legacy::BlockCipher &bc, const Bytes &in) {
        Bytes out(in.size());
        for (size_t i = 0; i < in.size(); i += 8)
            bc.encrypt(in.data() + i, out.data() + i);
        return out;
    };
    for (int round = 0; round < 20; ++round) {
        Bytes data = random_bytes(64);
        Bytes k8 = random_bytes(8), k16 = random_bytes(16), k24 = random_bytes(24);
        CHECK(ecb(*legacy::make_des(k8.data()), data) == ref("DES-ECB", k8, 0, data));
        CHECK(ecb(*legacy::make_des3(k16.data(), 16), data) == ref("DES-EDE", k16, 0, data));
        CHECK(ecb(*legacy::make_des3(k24.data(), 24), data) == ref("DES-EDE3", k24, 0, data));
        CHECK(ecb(*legacy::make_idea(k16.data()), data) == ref("IDEA-ECB", k16, 0, data));
        size_t rl = size_t(1 + round % 16);
        Bytes kr = random_bytes(rl);
        CHECK(ecb(*legacy::make_rc2(kr.data(), kr.size(), unsigned(rl * 8)), data) == ref("RC2-ECB", kr, unsigned(rl * 8), data));
        Bytes rc4out(data.size());
        legacy::rc4(kr.data(), kr.size(), data.data(), rc4out.data(), data.size());
        CHECK(rc4out == ref("RC4", kr, 0, data));
    }
    OSSL_LIB_CTX_free(ctx);
}

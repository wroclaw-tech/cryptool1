#include "legacy.hpp"
#include "testing.hpp"

#include <openssl/provider.h>

using namespace compat;

namespace {

struct Vector {
    AlgId *aid;
    const char *msg;
    const char *digest;
};

const char kLong[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

std::vector<Vector> vectors()
{
    return {
        {&md2_aid, "", "8350e5a3e24c153df2275c9f80692773"},
        {&md2_aid, "abc", "da853b0d3f88d99b30283a69e6ded6bb"},
        {&md2_aid, "message digest", "ab4f496bfb2a530b219ff33031fe06b0"},
        {&md2_aid, "abcdefghijklmnopqrstuvwxyz", "4e8ddff3650292ab5a4108c3aa47940b"},
        {&md4_aid, "", "31d6cfe0d16ae931b73c59d7e0c089c0"},
        {&md4_aid, "abc", "a448017aaf21d8525fc10ae87aa6729d"},
        {&md4_aid, "message digest", "d9130a8164549fe818874806e1c7014b"},
        {&md5_aid, "", "d41d8cd98f00b204e9800998ecf8427e"},
        {&md5_aid, "abc", "900150983cd24fb0d6963f7d28e17f72"},
        {&md5_aid, "message digest", "f96b697d7cb7938d525a2f31aaf161d0"},
        {&sha_aid, "abc", "0164b8a914cd2a5e74c4f7ff082c4d97f1edf880"},
        {&sha_aid, kLong, "d2516ee1acfa5baf33dfc1c471e438449ef134c8"},
        {&sha1_aid, "abc", "a9993e364706816aba3e25717850c26c9cd0d89d"},
        {&sha1_aid, kLong, "84983e441c3bd26ebaae4aa1f95129e5e54670f1"},
        {&ripemd160_aid, "", "9c1185a5c5e9fc54612808977ee8f548b2258d31"},
        {&ripemd160_aid, "abc", "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc"},
        {&ripemd160_aid, kLong, "12a053384a9c0c88e405a06c27dcf49ada62eb2b"},
    };
}

OctetString ostr(const char *s)
{
    return OctetString{sec_uint4(std::strlen(s)), const_cast<char *>(s)};
}

} // namespace

TEST(hash_known_answers_all)
{
    for (const auto &v : vectors()) {
        OctetString in = ostr(v.msg);
        OctetString out{0, nullptr};
        CHECK_EQ(sec_hash_all(&in, &out, v.aid, nullptr), 0);
        CHECK_HEX(out.octets, out.noctets, v.digest);
        aux_free2_OctetString(&out);
    }
}

TEST(hash_known_answers_incremental)
{
    for (const auto &v : vectors()) {
        void *ctx = nullptr;
        CHECK_EQ(sec_hash_init(&ctx, v.aid, nullptr), 0);
        size_t n = std::strlen(v.msg);
        for (size_t i = 0; i < n; i += 3) {
            OctetString part{sec_uint4(std::min<size_t>(3, n - i)), const_cast<char *>(v.msg + i)};
            CHECK_EQ(sec_hash_more(&ctx, &part), 0);
        }
        OctetString out{0, nullptr};
        CHECK_EQ(sec_hash_end(&ctx, &out), 0);
        CHECK(ctx == nullptr);
        CHECK_HEX(out.octets, out.noctets, v.digest);
        aux_free2_OctetString(&out);
    }
}

TEST(hash_in_place_like_pkcs5_dialog)
{
    OctetString h{0, nullptr};
    OctetString in = ostr("abc");
    CHECK_EQ(sec_hash_all(&in, &h, &sha1_aid, nullptr), 0);
    CHECK_EQ(sec_hash_all(&h, &h, &sha1_aid, nullptr), 0);
    Bytes once = digest(Hash::SHA1, reinterpret_cast<const uint8_t *>("abc"), 3);
    Bytes twice = digest(Hash::SHA1, once.data(), once.size());
    CHECK_HEX(h.octets, h.noctets, testing::hex(twice.data(), twice.size()));
}

TEST(hash_million_a)
{
    std::string a(1000000, 'a');
    OctetString in{sec_uint4(a.size()), &a[0]};
    OctetString out{0, nullptr};
    CHECK_EQ(sec_hash_all(&in, &out, &sha1_aid, nullptr), 0);
    CHECK_HEX(out.octets, out.noctets, "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    CHECK_EQ(sec_hash_all(&in, &out, &md5_aid, nullptr), 0);
    CHECK_HEX(out.octets, out.noctets, "7707d6ae4e027c70eea2a935c2296f21");
    CHECK_EQ(sec_hash_all(&in, &out, &ripemd160_aid, nullptr), 0);
    CHECK_HEX(out.octets, out.noctets, "52783243c1697bdbe16d37f97f68f08325dc1528");
}

TEST(hash_context_apis)
{
    unsigned char d[16];
    SEC_MD2_CTX m2;
    sec_MD2Init(&m2);
    sec_MD2Update(&m2, (unsigned char *)"message ", 8);
    sec_MD2Update(&m2, (unsigned char *)"digest", 6);
    sec_MD2Final(d, &m2);
    CHECK_HEX(d, 16, "ab4f496bfb2a530b219ff33031fe06b0");
    SEC_MD4_CTX m4;
    sec_MD4Init(&m4);
    sec_MD4Update(&m4, (unsigned char *)"abc", 3);
    sec_MD4Final(d, &m4);
    CHECK_HEX(d, 16, "a448017aaf21d8525fc10ae87aa6729d");
    SEC_MD5_CTX m5;
    sec_MD5Init(&m5);
    sec_MD5Update(&m5, (unsigned char *)"message digest", 14);
    sec_MD5Final(d, &m5);
    CHECK_HEX(d, 16, "f96b697d7cb7938d525a2f31aaf161d0");

    SHS_INFO s;
    unsigned char be[20];
    shsInit(&s);
    shsUpdate(&s, (unsigned char *)"ab", 2);
    shsUpdate(&s, (unsigned char *)"c", 1);
    shsFinal(&s);
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 4; ++j)
            be[4 * i + j] = (unsigned char)(s.digest[i] >> (24 - 8 * j));
    CHECK_HEX(be, 20, "0164b8a914cd2a5e74c4f7ff082c4d97f1edf880");
    shs1Init(&s);
    shs1Update(&s, (unsigned char *)kLong, sec_int4(std::strlen(kLong)));
    shs1Final(&s);
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 4; ++j)
            be[4 * i + j] = (unsigned char)(s.digest[i] >> (24 - 8 * j));
    CHECK_HEX(be, 20, "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}

TEST(hash_builtin_matches_openssl)
{
    OSSL_LIB_CTX *ctx = OSSL_LIB_CTX_new();
    OSSL_PROVIDER_load(ctx, "default");
    bool legacy = OSSL_PROVIDER_load(ctx, "legacy") != nullptr;
    struct {
        Hash h;
        const char *name;
    } algs[] = {{Hash::MD4, "MD4"}, {Hash::MD5, "MD5"}, {Hash::SHA1, "SHA1"}, {Hash::RIPEMD160, "RIPEMD160"}};
    for (size_t len : {0u, 1u, 55u, 56u, 63u, 64u, 65u, 1000u}) {
        Bytes msg = random_bytes(len);
        for (const auto &a : algs) {
            EVP_MD *md = EVP_MD_fetch(ctx, a.name, nullptr);
            if (!md) {
                CHECK(a.h == Hash::MD4 && !legacy);
                continue;
            }
            unsigned char ref[EVP_MAX_MD_SIZE];
            unsigned int n = 0;
            EVP_Digest(msg.data(), msg.size(), ref, &n, md, nullptr);
            EVP_MD_free(md);
            auto own = legacy::make_digest(a.h);
            own->update(msg.data(), msg.size());
            Bytes d = own->final();
            CHECK_HEX(d.data(), d.size(), testing::hex(ref, n));
        }
    }
    OSSL_LIB_CTX_free(ctx);
}

TEST(hash_unknown_algorithm_reports_error)
{
    OctetString in = ostr("abc");
    OctetString out{0, nullptr};
    CHECK_EQ(sec_hash_all(&in, &out, &desECB_aid, nullptr), -1);
    CHECK_EQ(th_last_error(), EUNKNOWNALGID);
    aux_free_error();
}

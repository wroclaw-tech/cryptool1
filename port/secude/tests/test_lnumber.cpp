#include "bn.hpp"
#include "testing.hpp"

#include <openssl/err.h>

using namespace compat;

namespace {

struct Ln {
    L_NUMBER v[MAXGENL];
    Ln() { std::memset(v, 0, sizeof v); }
};

Ln ln_from(const BIGNUM *b)
{
    Ln l;
    Bytes be = bn_bytes(b);
    OctetString o{sec_uint4(be.size()), reinterpret_cast<char *>(be.data())};
    CHECK_EQ(aux_OctetString2LN2(l.v, &o), 0);
    if (BN_is_negative(b))
        l.v[0] = L_NUMBER(~l.v[0] + 1);
    return l;
}

Bn bn_of(const Ln &l)
{
    Ln copy = l;
    bool neg = copy.v[0] & 0x80000000u;
    if (neg)
        copy.v[0] = L_NUMBER(~copy.v[0] + 1);
    OctetString *o = aux_LN2OctetString(copy.v, 0);
    Bn b = bn_from(bytes_of(o));
    aux_free_OctetString(&o);
    BN_set_negative(b.get(), neg);
    return b;
}

Bn rnd(int bits)
{
    Bn b = bn_new();
    if (bits > 0)
        BN_rand(b.get(), bits, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);
    return b;
}

bool same(const BIGNUM *a, const Ln &l)
{
    return BN_cmp(a, bn_of(l).get()) == 0;
}

} // namespace

TEST(lnumber_layout)
{
    uint8_t be[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    OctetString o{5, reinterpret_cast<char *>(be)};
    L_NUMBER l[MAXLGTH];
    CHECK_EQ(aux_OctetString2LN2(l, &o), 0);
    CHECK_EQ(l[0], L_NUMBER(2));
    CHECK_EQ(l[1], L_NUMBER(0x02030405));
    CHECK_EQ(l[2], L_NUMBER(0x01));
    CHECK_EQ(lngtouse(l), 32);
    L_NUMBER zero[1] = {0};
    CHECK_EQ(lngtouse(zero), -1);
    OctetString *back = aux_LN2OctetString(l, 8);
    CHECK_HEX(back->octets, back->noctets, "0000000102030405");
    aux_free_OctetString(&back);
    L_NUMBER big[3] = {2, 0, 0x01000000};
    back = aux_LN2OctetString(big, 2);
    CHECK_HEX(back->octets, back->noctets, "0100000000000000");
    aux_free_OctetString(&back);
}

TEST(lnumber_bitstring_and_random)
{
    uint8_t bits[] = {0xab, 0xcd};
    BitString b{12, reinterpret_cast<char *>(bits)};
    L_NUMBER l[MAXLGTH];
    CHECK_EQ(aux_BitString2LN2(l, &b), 0);
    CHECK_EQ(l[0], L_NUMBER(1));
    CHECK_EQ(l[1], L_NUMBER(0xabc));
    for (int i = 0; i < 50; ++i) {
        CHECK_EQ(rndm(1, l, 0), 0);
        CHECK(l[0] == 0 || (l[0] == 1 && l[1] == 1));
        CHECK_EQ(rndm(100, l, 0), 0);
        CHECK(lngtouse(l) < 100);
    }
}

TEST(lnumber_matches_bignum)
{
    BN_CTX *ctx = BN_CTX_new();
    for (int round = 0; round < 300; ++round) {
        int sizes[] = {0, 1, 31, 32, 33, 64, 160, 239, 512, 1024, 2048};
        Bn a = rnd(sizes[round % 11]), b = rnd(sizes[(round / 11) % 11]);
        Bn m = rnd(239 + round % 300);
        BN_set_bit(m.get(), 0);
        if (BN_is_zero(m.get()) || BN_is_one(m.get()))
            BN_set_word(m.get(), 65537);
        Ln la = ln_from(a.get()), lb = ln_from(b.get()), lm = ln_from(m.get()), r, q;
        Bn e = bn_new();

        CHECK_EQ(arithmetic_comp(la.v, lb.v), BN_cmp(a.get(), b.get()));
        arithmetic_add(la.v, lb.v, r.v);
        BN_add(e.get(), a.get(), b.get());
        CHECK(same(e.get(), r));
        arithmetic_sub(la.v, lb.v, r.v);
        BN_sub(e.get(), a.get(), b.get());
        CHECK(same(e.get(), r));
        arithmetic_mult(la.v, lb.v, r.v);
        BN_mul(e.get(), a.get(), b.get(), ctx);
        CHECK(same(e.get(), r));
        if (!BN_is_zero(b.get())) {
            Bn eq = bn_new();
            arithmetic_div(la.v, lb.v, q.v, r.v);
            BN_div(eq.get(), e.get(), a.get(), b.get(), ctx);
            CHECK(same(eq.get(), q));
            CHECK(same(e.get(), r));
        }
        arithmetic_shift(la.v, 37, r.v);
        BN_lshift(e.get(), a.get(), 37);
        CHECK(same(e.get(), r));
        arithmetic_shift(la.v, -5, r.v);
        BN_rshift(e.get(), a.get(), 5);
        CHECK(same(e.get(), r));

        CHECK_EQ(arithmetic_madd(la.v, lb.v, r.v, lm.v), 0);
        BN_mod_add(e.get(), a.get(), b.get(), m.get(), ctx);
        CHECK(same(e.get(), r));
        CHECK_EQ(arithmetic_msub(la.v, lb.v, r.v, lm.v), 0);
        BN_mod_sub(e.get(), a.get(), b.get(), m.get(), ctx);
        CHECK(same(e.get(), r));
        CHECK_EQ(arithmetic_mmult(la.v, lb.v, r.v, lm.v), 0);
        BN_mod_mul(e.get(), a.get(), b.get(), m.get(), ctx);
        CHECK(same(e.get(), r));
        CHECK_EQ(arithmetic_mexp(la.v, lb.v, r.v, lm.v), 0);
        BN_mod_exp(e.get(), a.get(), b.get(), m.get(), ctx);
        CHECK(same(e.get(), r));
        Bn inv = bn_new();
        ERR_set_mark();
        bool invertible = BN_mod_inverse(inv.get(), b.get(), m.get(), ctx) != nullptr;
        ERR_pop_to_mark();
        int rc = arithmetic_mdiv(la.v, lb.v, r.v, lm.v);
        if (invertible) {
            CHECK_EQ(rc, 0);
            BN_mod_mul(e.get(), a.get(), inv.get(), m.get(), ctx);
            CHECK(same(e.get(), r));
        } else {
            CHECK_EQ(rc, -1);
        }
    }
    aux_free_error();
    BN_CTX_free(ctx);
}

TEST(lnumber_aliasing_like_libec)
{
    Bn a = rnd(300), m = rnd(301);
    BN_set_bit(m.get(), 300);
    Ln la = ln_from(a.get()), lm = ln_from(m.get());
    arithmetic_mmult(la.v, la.v, la.v, lm.v);
    Bn e = bn_new();
    BN_CTX *ctx = BN_CTX_new();
    BN_mod_mul(e.get(), a.get(), a.get(), m.get(), ctx);
    CHECK(same(e.get(), la));
    Ln n = ln_from(e.get()), tmp;
    arithmetic_div(n.v, lm.v, tmp.v, n.v);
    BN_nnmod(e.get(), e.get(), m.get(), ctx);
    CHECK(same(e.get(), n));
    BN_CTX_free(ctx);
}

TEST(lnumber_zero_result_clears_first_word)
{
    // libec's ln_to_string reads rem[1] without looking at rem[0]
    L_NUMBER a[MAXLGTH] = {1, 32}, b[MAXLGTH] = {1, 16}, q[MAXLGTH], r[MAXLGTH] = {1, 0xdeadbeef};
    arithmetic_div(a, b, q, r);
    CHECK_EQ(q[1], L_NUMBER(2));
    CHECK_EQ(r[0], L_NUMBER(0));
    CHECK_EQ(r[1], L_NUMBER(0));
}

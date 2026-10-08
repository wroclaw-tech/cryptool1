// SECUDE long-number arithmetic on L_NUMBER arrays:
//   a[0]    number of 32-bit words (two's complement of it for negative values)
//   a[1..n] magnitude, least significant word first
#include "bn.hpp"

namespace compat {

namespace {

BN_CTX *ctx() { return bn_ctx(); }

constexpr L_NUMBER kSign = 0x80000000u;
constexpr size_t kMaxWords = MAXGENL * 4;

Bn from_ln(const L_NUMBER *a)
{
    if (!a)
        fail(EINVALID, "missing L_NUMBER");
    L_NUMBER head = a[0];
    bool negative = head & kSign;
    size_t n = negative ? size_t(L_NUMBER(~head + 1)) : size_t(head);
    if (n > kMaxWords)
        fail(EINVALID, "L_NUMBER too long");
    std::vector<uint8_t> be(n * 4);
    for (size_t i = 0; i < n; ++i) {
        L_NUMBER w = a[n - i];
        be[4 * i] = uint8_t(w >> 24);
        be[4 * i + 1] = uint8_t(w >> 16);
        be[4 * i + 2] = uint8_t(w >> 8);
        be[4 * i + 3] = uint8_t(w);
    }
    Bn b = bn_new();
    if (!BN_bin2bn(be.data(), int(be.size()), b.get()))
        throw std::bad_alloc();
    BN_set_negative(b.get(), negative && !BN_is_zero(b.get()));
    return b;
}

void to_ln(const BIGNUM *b, L_NUMBER *out)
{
    int nbytes = BN_num_bytes(b);
    size_t n = size_t(nbytes + 3) / 4;
    std::vector<uint8_t> be(n * 4);
    if (n && BN_bn2binpad(b, be.data(), int(be.size())) < 0)
        fail(EINTERNAL, "BN_bn2binpad failed");
    for (size_t i = 0; i < n; ++i) {
        size_t off = (n - 1 - i) * 4;
        out[i + 1] = (L_NUMBER(be[off]) << 24) | (L_NUMBER(be[off + 1]) << 16) | (L_NUMBER(be[off + 2]) << 8) | be[off + 3];
    }
    out[0] = BN_is_negative(b) ? L_NUMBER(~L_NUMBER(n) + 1) : L_NUMBER(n);
    // libec reads x[1] of a zero result (e.g. the remainder in ln_to_string)
    if (n == 0)
        out[1] = 0;
}

void check(int ok) { bn_check(ok); }

} // namespace

} // namespace compat

using namespace compat;

extern "C" {

int lngtouse(L_NUMBER_ARRAY modul)
{
    return guarded<int>("lngtouse", -1, [&] { return BN_num_bits(from_ln(modul).get()) - 1; });
}

int arithmetic_comp(L_NUMBER *Ap, L_NUMBER *Bp)
{
    return guarded<int>("arithmetic_comp", 0, [&] {
        int c = BN_cmp(from_ln(Ap).get(), from_ln(Bp).get());
        return c < 0 ? -1 : c > 0 ? 1 : 0;
    });
}

void arithmetic_add(L_NUMBER_ARRAY Ap, L_NUMBER_ARRAY Bp, L_NUMBER_ARRAY Sum)
{
    guarded_void("arithmetic_add", [&] {
        Bn a = from_ln(Ap), b = from_ln(Bp), r = bn_new();
        check(BN_add(r.get(), a.get(), b.get()));
        to_ln(r.get(), Sum);
    });
}

void arithmetic_sub(L_NUMBER_ARRAY Ap, L_NUMBER_ARRAY Bp, L_NUMBER_ARRAY Diff)
{
    guarded_void("arithmetic_sub", [&] {
        Bn a = from_ln(Ap), b = from_ln(Bp), r = bn_new();
        check(BN_sub(r.get(), a.get(), b.get()));
        to_ln(r.get(), Diff);
    });
}

void arithmetic_mult(L_NUMBER_ARRAY A, L_NUMBER_ARRAY B, L_NUMBER_ARRAY erg)
{
    guarded_void("arithmetic_mult", [&] {
        Bn a = from_ln(A), b = from_ln(B), r = bn_new();
        check(BN_mul(r.get(), a.get(), b.get(), ctx()));
        to_ln(r.get(), erg);
    });
}

void arithmetic_div(L_NUMBER_ARRAY A, L_NUMBER_ARRAY B, L_NUMBER_ARRAY Q, L_NUMBER_ARRAY R)
{
    guarded_void("arithmetic_div", [&] {
        Bn a = from_ln(A), b = from_ln(B), q = bn_new(), r = bn_new();
        if (BN_is_zero(b.get()))
            fail(EINVALID, "division by zero");
        check(BN_div(q.get(), r.get(), a.get(), b.get(), ctx()));
        if (Q)
            to_ln(q.get(), Q);
        if (R)
            to_ln(r.get(), R);
    });
}

void arithmetic_shift(L_NUMBER_ARRAY A, int exp2b, L_NUMBER_ARRAY S)
{
    guarded_void("arithmetic_shift", [&] {
        Bn a = from_ln(A), r = bn_new();
        bool negative = BN_is_negative(a.get());
        BN_set_negative(a.get(), 0);
        if (exp2b >= 0)
            check(BN_lshift(r.get(), a.get(), exp2b));
        else
            check(BN_rshift(r.get(), a.get(), -exp2b));
        BN_set_negative(r.get(), negative && !BN_is_zero(r.get()));
        to_ln(r.get(), S);
    });
}

static int modular(const char *proc, L_NUMBER *op1, L_NUMBER *op2, L_NUMBER *erg, L_NUMBER *modul,
                   int (*fn)(BIGNUM *, const BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *))
{
    return guarded<int>(proc, -1, [&] {
        Bn a = from_ln(op1), b = from_ln(op2), m = from_ln(modul), r = bn_new();
        if (BN_is_zero(m.get()))
            fail(EINVALID, "modulus is zero");
        BN_set_negative(m.get(), 0);
        check(fn(r.get(), a.get(), b.get(), m.get(), ctx()));
        to_ln(r.get(), erg);
        return 0;
    });
}

int arithmetic_madd(L_NUMBER_ARRAY op1, L_NUMBER_ARRAY op2, L_NUMBER_ARRAY erg, L_NUMBER_ARRAY modul)
{
    return modular("arithmetic_madd", op1, op2, erg, modul, BN_mod_add);
}

int arithmetic_msub(L_NUMBER_ARRAY op1, L_NUMBER_ARRAY op2, L_NUMBER_ARRAY erg, L_NUMBER_ARRAY modul)
{
    return modular("arithmetic_msub", op1, op2, erg, modul, BN_mod_sub);
}

int arithmetic_mmult(L_NUMBER_ARRAY op1, L_NUMBER_ARRAY op2, L_NUMBER_ARRAY erg, L_NUMBER_ARRAY modul)
{
    return modular("arithmetic_mmult", op1, op2, erg, modul, BN_mod_mul);
}

int arithmetic_mdiv(L_NUMBER_ARRAY op1, L_NUMBER_ARRAY op2, L_NUMBER_ARRAY erg, L_NUMBER_ARRAY modul)
{
    return guarded<int>("arithmetic_mdiv", -1, [&] {
        Bn a = from_ln(op1), b = from_ln(op2), m = from_ln(modul), inv = bn_new(), r = bn_new();
        if (BN_is_zero(m.get()))
            fail(EINVALID, "modulus is zero");
        BN_set_negative(m.get(), 0);
        if (!BN_mod_inverse(inv.get(), b.get(), m.get(), ctx()))
            fail(EINVALID, "divisor not invertible");
        check(BN_mod_mul(r.get(), a.get(), inv.get(), m.get(), ctx()));
        to_ln(r.get(), erg);
        return 0;
    });
}

int arithmetic_mexp(L_NUMBER_ARRAY bas, L_NUMBER_ARRAY exp, L_NUMBER_ARRAY erg, L_NUMBER_ARRAY modul)
{
    return guarded<int>("arithmetic_mexp", -1, [&] {
        Bn b = from_ln(bas), e = from_ln(exp), m = from_ln(modul), r = bn_new();
        if (BN_is_zero(m.get()))
            fail(EINVALID, "modulus is zero");
        if (BN_is_negative(e.get()))
            fail(EINVALID, "negative exponent");
        BN_set_negative(m.get(), 0);
        check(BN_nnmod(b.get(), b.get(), m.get(), ctx()));
        check(BN_mod_exp(r.get(), b.get(), e.get(), m.get(), ctx()));
        to_ln(r.get(), erg);
        return 0;
    });
}

// 0 for a probable prime, -1 otherwise (libec's curve validation)
int rabinstest(L_NUMBER_ARRAY zahl)
{
    return guarded<int>("rabinstest", -1, [&] {
        Bn n = from_ln(zahl);
        if (BN_is_negative(n.get()))
            return -1;
        int r = BN_check_prime(n.get(), ctx(), nullptr);
        if (r < 0)
            fail(EINTERNAL, "primality test failed");
        return r == 1 ? 0 : -1;
    });
}

int rndm(int lgth, L_NUMBER_ARRAY zahl, int version)
{
    return guarded<int>("rndm", -1, [&] {
        Bn r = bn_new();
        if (lgth <= 0)
            BN_zero(r.get());
        else
            check(BN_rand(r.get(), lgth, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY));
        to_ln(r.get(), zahl);
        return 0;
    });
}

RC aux_OctetString2LN2(L_NUMBER *lnum, OctetString *ostr)
{
    return guarded<RC>("aux_OctetString2LN2", -1, [&] {
        if (!lnum || !ostr)
            fail(EINVALID, "missing parameter");
        Bytes b = bytes_of(ostr);
        Bn r = bn_new();
        if (!b.empty() && !BN_bin2bn(b.data(), int(b.size()), r.get()))
            throw std::bad_alloc();
        to_ln(r.get(), lnum);
        return 0;
    });
}

RC aux_BitString2LN2(L_NUMBER *lnum, BitString *bstr)
{
    return guarded<RC>("aux_BitString2LN2", -1, [&] {
        if (!lnum || !bstr)
            fail(EINVALID, "missing parameter");
        Bytes b = bytes_of(bstr);
        Bn r = bn_new();
        if (!b.empty()) {
            if (!BN_bin2bn(b.data(), int(b.size()), r.get()))
                throw std::bad_alloc();
            check(BN_rshift(r.get(), r.get(), int(b.size() * 8 - bstr->nbits)));
        }
        to_ln(r.get(), lnum);
        return 0;
    });
}

OctetString *aux_LN2OctetString(L_NUMBER_ARRAY lnum, int size)
{
    return guarded<OctetString *>("aux_LN2OctetString", nullptr, [&] {
        Bn a = from_ln(lnum);
        size_t minimal = size_t(BN_num_bytes(a.get()));
        size_t n = size > 0 && size_t(size) > minimal ? size_t(size) : minimal;
        Bytes out(n);
        if (n)
            BN_bn2binpad(a.get(), out.data(), int(n));
        return new_ostr(out);
    });
}

} // extern "C"

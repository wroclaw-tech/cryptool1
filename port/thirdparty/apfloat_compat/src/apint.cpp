#include "apint.h"

#include <atomic>
#include <cctype>
#include <climits>
#include <cmath>
#include <istream>
#include <ostream>
#include <stdexcept>

namespace {

std::atomic<int> g_base{10};

constexpr long kMaxStringExponent = 100000000L;

int digit_value(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return INT_MAX;
}

void set_magnitude(mpz_class &out, unsigned long long v)
{
    mpz_import(out.get_mpz_t(), 1, 1, sizeof v, 0, 0, &v);
}

void require_nonzero(const apint &d)
{
    if (sgn(d.val) == 0)
        throw std::domain_error("apint: division by zero");
}

}  // namespace

bool apinit(void)
{
    g_base = 10;
    return true;
}

void apdeinit(void)
{
}

void apbase(int digit)
{
    if (digit >= 2 && digit <= 36)
        g_base = digit;
}

apint::apint(double newval)
{
    if (std::isfinite(newval))
        mpz_set_d(val.get_mpz_t(), std::floor(newval));
}

void apint::setll(long long v)
{
    if (v < 0) {
        set_magnitude(val, 0ULL - static_cast<unsigned long long>(v));
        val = -val;
    } else {
        set_magnitude(val, static_cast<unsigned long long>(v));
    }
}

void apint::setull(unsigned long long v)
{
    set_magnitude(val, v);
}

// Parses [ws][sign]digits[.digits][e[sign]digits] in the current base and
// rounds toward negative infinity, like apint(char *) = floor(apfloat(char *)).
void apint::setstr(const char *s)
{
    val = 0;
    if (!s)
        return;
    const int base = g_base;
    const unsigned char *p = reinterpret_cast<const unsigned char *>(s);
    while (std::isspace(*p))
        ++p;
    bool negative = false;
    if (*p == '+' || *p == '-')
        negative = (*p++ == '-');

    std::string digits;
    long fraction_digits = 0;
    bool seen_point = false;
    for (;; ++p) {
        if (*p == '.' && !seen_point) {
            seen_point = true;
            continue;
        }
        if (digit_value(*p) >= base)
            break;
        digits.push_back(static_cast<char>(std::tolower(*p)));
        if (seen_point)
            ++fraction_digits;
    }
    if (digits.empty())
        return;

    long exponent = 0;
    if (base <= 10 && (*p == 'e' || *p == 'E')) {
        const unsigned char *q = p + 1;
        bool exp_negative = false;
        if (*q == '+' || *q == '-')
            exp_negative = (*q++ == '-');
        if (std::isdigit(*q)) {
            while (std::isdigit(*q)) {
                if (exponent < kMaxStringExponent)
                    exponent = exponent * 10 + (*q - '0');
                ++q;
            }
            if (exp_negative)
                exponent = -exponent;
        }
    }

    mpz_class mantissa;
    if (mantissa.set_str(digits, base) != 0)
        return;
    if (negative)
        mantissa = -mantissa;

    const long shift = exponent - fraction_digits;
    if (shift >= 0) {
        mpz_class scale;
        mpz_ui_pow_ui(scale.get_mpz_t(), static_cast<unsigned long>(base), static_cast<unsigned long>(shift));
        val = mantissa * scale;
    } else {
        mpz_class scale;
        mpz_ui_pow_ui(scale.get_mpz_t(), static_cast<unsigned long>(base), static_cast<unsigned long>(-shift));
        mpz_fdiv_q(val.get_mpz_t(), mantissa.get_mpz_t(), scale.get_mpz_t());
    }
}

void apint::sign(int newsign)
{
    if (newsign == 0)
        val = 0;
    else if ((newsign < 0) != (sgn(val) < 0))
        val = -val;
}

std::string apint::toString(int base) const
{
    if (base < 2 || base > 36)
        base = g_base;
    std::string out(mpz_sizeinbase(val.get_mpz_t(), base) + 2, '\0');
    mpz_get_str(&out[0], base, val.get_mpz_t());
    out.resize(std::char_traits<char>::length(out.c_str()));
    return out;
}

apint operator/ (const apint &d1, const apint &d2)
{
    require_nonzero(d2);
    apint r;
    mpz_tdiv_q(r.val.get_mpz_t(), d1.val.get_mpz_t(), d2.val.get_mpz_t());
    return r;
}

apint operator% (const apint &d1, const apint &d2)
{
    require_nonzero(d2);
    apint r;
    mpz_tdiv_r(r.val.get_mpz_t(), d1.val.get_mpz_t(), d2.val.get_mpz_t());
    return r;
}

apint operator<< (const apint &d1, size_t d2)
{
    apint r;
    mpz_mul_2exp(r.val.get_mpz_t(), d1.val.get_mpz_t(), static_cast<mp_bitcnt_t>(d2));
    return r;
}

apint operator>> (const apint &d1, size_t d2)
{
    apint r;
    mpz_tdiv_q_2exp(r.val.get_mpz_t(), d1.val.get_mpz_t(), static_cast<mp_bitcnt_t>(d2));
    return r;
}

std::ostream &operator<< (std::ostream &str, const apint &d)
{
    return str << d.toString();
}

std::istream &operator>> (std::istream &str, apint &d)
{
    std::string token;
    if (str >> token)
        d = token;
    return str;
}

apint pow (apint base, unsigned long exp)
{
    apint r;
    mpz_pow_ui(r.val.get_mpz_t(), base.val.get_mpz_t(), exp);
    return r;
}

apint pow (apint base, unsigned exp)
{
    return pow(base, static_cast<unsigned long>(exp));
}

apint pow (apint base, long exp)
{
    if (exp >= 0)
        return pow(base, static_cast<unsigned long>(exp));
    require_nonzero(base);
    if (base == 1)
        return apint(1);
    if (base == -1)
        return apint((exp & 1) ? -1 : 1);
    return apint(0);
}

apint pow (apint base, int exp)
{
    return pow(base, static_cast<long>(exp));
}

apint abs (apint x)
{
    mpz_abs(x.val.get_mpz_t(), x.val.get_mpz_t());
    return x;
}

apdiv_t div (apint numer, apint denom)
{
    require_nonzero(denom);
    apdiv_t r;
    mpz_tdiv_qr(r.quot.val.get_mpz_t(), r.rem.val.get_mpz_t(), numer.val.get_mpz_t(), denom.val.get_mpz_t());
    return r;
}

apint gcd (apint a, apint b)
{
    apint r;
    mpz_gcd(r.val.get_mpz_t(), a.val.get_mpz_t(), b.val.get_mpz_t());
    return r;
}

apint lcm (apint a, apint b)
{
    apint r;
    mpz_lcm(r.val.get_mpz_t(), a.val.get_mpz_t(), b.val.get_mpz_t());
    return r;
}

apint powmod (apint base, apint exp, apint modulus)
{
    require_nonzero(modulus);
    apint r;
    if (sgn(exp.val) < 0) {
        mpz_class inv;
        if (!mpz_invert(inv.get_mpz_t(), base.val.get_mpz_t(), modulus.val.get_mpz_t()))
            throw std::domain_error("apint: powmod base not invertible");
        mpz_class e = -exp.val;
        mpz_powm(r.val.get_mpz_t(), inv.get_mpz_t(), e.get_mpz_t(), modulus.val.get_mpz_t());
        return r;
    }
    mpz_powm(r.val.get_mpz_t(), base.val.get_mpz_t(), exp.val.get_mpz_t(), modulus.val.get_mpz_t());
    return r;
}

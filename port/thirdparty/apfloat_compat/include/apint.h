// Source-compatible subset of apfloat 2.41's apint, backed by GMP's mpz_class.
// Division and remainder truncate toward zero; conversions from double and
// strings with a fractional part round toward negative infinity (floor).
#ifndef __cplusplus
#error Must use C++ for the type apint.
#endif

#if !defined(__APINT_H)
#define __APINT_H

#include <cstddef>
#include <iosfwd>
#include <string>
#include <type_traits>

#include <gmpxx.h>

#include "ap.h"

class apint
{
    template <class T>
    using if_integral = typename std::enable_if<std::is_integral<T>::value, int>::type;

public:
    apint () {}
    apint (int newval) : val (static_cast<long> (newval)) {}
    apint (unsigned newval) : val (static_cast<unsigned long> (newval)) {}
    apint (long newval) : val (newval) {}
    apint (unsigned long newval) : val (newval) {}
    apint (long long newval) { setll (newval); }
    apint (unsigned long long newval) { setull (newval); }
    apint (double newval);
    apint (const char *newval) { setstr (newval); }
    apint (const std::string &newval) { setstr (newval.c_str ()); }
    explicit apint (const mpz_class &newval) : val (newval) {}

    apint &operator= (int d) { val = static_cast<long> (d); return *this; }
    apint &operator= (unsigned d) { val = static_cast<unsigned long> (d); return *this; }
    apint &operator= (long d) { val = d; return *this; }
    apint &operator= (unsigned long d) { val = d; return *this; }
    apint &operator= (long long d) { setll (d); return *this; }
    apint &operator= (unsigned long long d) { setull (d); return *this; }
    apint &operator= (double d) { return *this = apint (d); }
    apint &operator= (const char *d) { setstr (d); return *this; }
    apint &operator= (const std::string &d) { setstr (d.c_str ()); return *this; }

    friend apint operator+ (const apint &d1, const apint &d2) { return apint (mpz_class (d1.val + d2.val)); }
    friend apint operator- (const apint &d1, const apint &d2) { return apint (mpz_class (d1.val - d2.val)); }
    friend apint operator* (const apint &d1, const apint &d2) { return apint (mpz_class (d1.val * d2.val)); }
    friend apint operator/ (const apint &d1, const apint &d2);
    friend apint operator% (const apint &d1, const apint &d2);
    friend apint operator<< (const apint &d1, size_t d2);
    friend apint operator>> (const apint &d1, size_t d2);

    friend bool operator== (const apint &d1, const apint &d2) { return d1.val == d2.val; }
    friend bool operator!= (const apint &d1, const apint &d2) { return d1.val != d2.val; }
    friend bool operator>= (const apint &d1, const apint &d2) { return d1.val >= d2.val; }
    friend bool operator<= (const apint &d1, const apint &d2) { return d1.val <= d2.val; }
    friend bool operator> (const apint &d1, const apint &d2) { return d1.val > d2.val; }
    friend bool operator< (const apint &d1, const apint &d2) { return d1.val < d2.val; }

    template <class T, if_integral<T> = 0> friend apint operator+ (const apint &d1, T d2) { return d1 + apint (d2); }
    template <class T, if_integral<T> = 0> friend apint operator+ (T d1, const apint &d2) { return apint (d1) + d2; }
    template <class T, if_integral<T> = 0> friend apint operator- (const apint &d1, T d2) { return d1 - apint (d2); }
    template <class T, if_integral<T> = 0> friend apint operator- (T d1, const apint &d2) { return apint (d1) - d2; }
    template <class T, if_integral<T> = 0> friend apint operator* (const apint &d1, T d2) { return d1 * apint (d2); }
    template <class T, if_integral<T> = 0> friend apint operator* (T d1, const apint &d2) { return apint (d1) * d2; }
    template <class T, if_integral<T> = 0> friend apint operator/ (const apint &d1, T d2) { return d1 / apint (d2); }
    template <class T, if_integral<T> = 0> friend apint operator/ (T d1, const apint &d2) { return apint (d1) / d2; }
    template <class T, if_integral<T> = 0> friend apint operator% (const apint &d1, T d2) { return d1 % apint (d2); }
    template <class T, if_integral<T> = 0> friend apint operator% (T d1, const apint &d2) { return apint (d1) % d2; }

    template <class T, if_integral<T> = 0> friend bool operator== (const apint &d1, T d2) { return d1 == apint (d2); }
    template <class T, if_integral<T> = 0> friend bool operator== (T d1, const apint &d2) { return apint (d1) == d2; }
    template <class T, if_integral<T> = 0> friend bool operator!= (const apint &d1, T d2) { return d1 != apint (d2); }
    template <class T, if_integral<T> = 0> friend bool operator!= (T d1, const apint &d2) { return apint (d1) != d2; }
    template <class T, if_integral<T> = 0> friend bool operator>= (const apint &d1, T d2) { return d1 >= apint (d2); }
    template <class T, if_integral<T> = 0> friend bool operator>= (T d1, const apint &d2) { return apint (d1) >= d2; }
    template <class T, if_integral<T> = 0> friend bool operator<= (const apint &d1, T d2) { return d1 <= apint (d2); }
    template <class T, if_integral<T> = 0> friend bool operator<= (T d1, const apint &d2) { return apint (d1) <= d2; }
    template <class T, if_integral<T> = 0> friend bool operator> (const apint &d1, T d2) { return d1 > apint (d2); }
    template <class T, if_integral<T> = 0> friend bool operator> (T d1, const apint &d2) { return apint (d1) > d2; }
    template <class T, if_integral<T> = 0> friend bool operator< (const apint &d1, T d2) { return d1 < apint (d2); }
    template <class T, if_integral<T> = 0> friend bool operator< (T d1, const apint &d2) { return apint (d1) < d2; }

    friend bool operator== (const apint &d1, double d2) { return cmpd (d1.val, d2) == 0; }
    friend bool operator== (double d1, const apint &d2) { return cmpd (d2.val, d1) == 0; }
    friend bool operator!= (const apint &d1, double d2) { return cmpd (d1.val, d2) != 0; }
    friend bool operator!= (double d1, const apint &d2) { return cmpd (d2.val, d1) != 0; }
    friend bool operator>= (const apint &d1, double d2) { return cmpd (d1.val, d2) >= 0; }
    friend bool operator>= (double d1, const apint &d2) { return cmpd (d2.val, d1) <= 0; }
    friend bool operator<= (const apint &d1, double d2) { return cmpd (d1.val, d2) <= 0; }
    friend bool operator<= (double d1, const apint &d2) { return cmpd (d2.val, d1) >= 0; }
    friend bool operator> (const apint &d1, double d2) { return cmpd (d1.val, d2) > 0; }
    friend bool operator> (double d1, const apint &d2) { return cmpd (d2.val, d1) < 0; }
    friend bool operator< (const apint &d1, double d2) { return cmpd (d1.val, d2) < 0; }
    friend bool operator< (double d1, const apint &d2) { return cmpd (d2.val, d1) > 0; }

    friend std::ostream &operator<< (std::ostream &, const apint &);
    friend std::istream &operator>> (std::istream &, apint &);

    apint &operator++ () { ++val; return *this; }
    apint &operator-- () { --val; return *this; }
    apint operator++ (int) { apint t (*this); ++val; return t; }
    apint operator-- (int) { apint t (*this); --val; return t; }
    apint &operator+= (const apint &d) { val += d.val; return *this; }
    apint &operator-= (const apint &d) { val -= d.val; return *this; }
    apint &operator*= (const apint &d) { val *= d.val; return *this; }
    apint &operator/= (const apint &d) { return *this = *this / d; }
    apint &operator%= (const apint &d) { return *this = *this % d; }
    apint &operator<<= (size_t d) { return *this = *this << d; }
    apint &operator>>= (size_t d) { return *this = *this >> d; }
    apint operator+ () const { return *this; }
    apint operator- () const { return apint (mpz_class (-val)); }

    int sign (void) const { return sgn (val); }
    void sign (int newsign);

    std::string toString (int base = 0) const;

    mpz_class val;

private:
    void setll (long long v);
    void setull (unsigned long long v);
    void setstr (const char *s);
    static int cmpd (const mpz_class &a, double b) { return mpz_cmp_d (a.get_mpz_t (), b); }
};

typedef struct apdiv_struct
{
    apint quot;
    apint rem;
} apdiv_t;

apint pow (apint base, unsigned long exp);
apint pow (apint base, unsigned exp);
apint pow (apint base, long exp);
apint pow (apint base, int exp);
apint abs (apint x);
apdiv_t div (apint numer, apint denom);
apint gcd (apint a, apint b);
apint lcm (apint a, apint b);
apint powmod (apint base, apint exp, apint modulus);

#endif  // __APINT_H

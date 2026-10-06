// Exercises the GMP/gmpxx API subset used by CrypTool (PrimeTest*, PrimePolynom, DlgAbout).
#include <gmp.h>
#include <gmpxx.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
                   #cond);                                                 \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

static mpz_class square_multiply(const mpz_class &a, const mpz_class &b, const mpz_class &n) {
  mpz_class x = 1, y = a, s = b;
  while (s > 0) {
    mpz_class s2 = s % 2;
    long odd = mpz_get_si(s2.get_mpz_t());
    if (odd == 1) {
      x = x * y;
      x = x % n;
    }
    s >>= 1;
    y = y * y;
    y = y % n;
  }
  return x;
}

int main() {
  mpz_class n("170141183460469231731687303715884105727");
  CHECK(mpz_probab_prime_p(n.get_mpz_t(), 10) > 0);
  CHECK(mpz_sizeinbase(n.get_mpz_t(), 2) == 127);

  mpz_class m = 561;
  CHECK(mpz_probab_prime_p(m.get_mpz_t(), 10) == 0);

  mpz_class pot_b = 1;
  pot_b = pot_b << 64;
  mpz_class u = pot_b * 12345 + 678;
  mpz_class coef = u % pot_b;
  CHECK(coef == 678);
  u = u >> 64;
  CHECK(mpz_get_ui(u.get_mpz_t()) == 12345UL);
  mpz_class c = 7;
  c <<= 1;
  CHECK(c == 14);

  CHECK(square_multiply(2, 10, 1000) == 24);
  CHECK(square_multiply(3, n - 1, n) == 1);

  mpz_class small = -42;
  CHECK(small.fits_sint_p());
  CHECK(small.get_si() == -42);
  CHECK(mpz_get_si(small.get_mpz_t()) == -42);

  std::string per_s = "99.9";
  int size = (int)std::ceil(3.321 * per_s.length());
  mpf_class per(per_s, size);
  mpf_class val(50.0, size + 1);
  mpf_class res(50.0, size + 1);
  int rounds = 1;
  while (per > res && rounds < 100) {
    val = val / 2;
    res = res + val;
    ++rounds;
  }
  CHECK(per >= 0 && per < 100);
  CHECK(rounds == 10);

  gmp_randclass rand(gmp_randinit_default);
  rand.seed(12345UL);
  mpz_class r = rand.get_z_range(n);
  CHECK(r >= 0 && r < n);

  char buf[64];
  gmp_snprintf(buf, sizeof buf, "%Zd", u.get_mpz_t());
  CHECK(std::strcmp(buf, "12345") == 0);
  std::vector<mpz_class> vec{1, 2, 3};
  for (auto it = vec.begin(); it != vec.end(); ++it)
    gmp_printf(" %Zd", (*it).mpz_class::get_mpz_t());
  std::printf("\n");

  CHECK(gmp_version != nullptr && gmp_version[0] >= '6');
  std::cout << "GMP " << gmp_version << ", 2^127-1 = " << n << std::endl;

  if (failures) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  std::printf("test_gmp: OK\n");
  return 0;
}

#include <NTL/ZZ.h>
#include <NTL/RR.h>
#include <NTL/LLL.h>
#include <NTL/ZZX.h>
#include <NTL/ZZXFactoring.h>
#include <NTL/ZZ_pX.h>
#include <NTL/mat_ZZ.h>
#include <NTL/matrix.h>
#include <NTL/vector.h>
#include <NTL/version.h>
#include <NTL/mach_desc.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

// Same pattern as CrypTool/ZZXY.h and ZZXY.cpp.
using namespace NTL;

NTL_vector_decl(vec_ZZX, vec_vec_ZZX)

NTL_matrix_decl(ZZX, vec_ZZX, vec_vec_ZZX, mat_ZZX)

NTL_vector_decl(vec_ZZ_pX, vec_vec_ZZ_pX)

NTL_matrix_decl(ZZ_pX, vec_ZZ_pX, vec_vec_ZZ_pX, mat_ZZ_pX)

NTL_vector_impl(vec_ZZX, vec_vec_ZZX)

NTL_matrix_impl(ZZX, vec_ZZX, vec_vec_ZZX, mat_ZZX)

NTL_vector_impl(vec_ZZ_pX, vec_vec_ZZ_pX)

NTL_matrix_impl(ZZ_pX, vec_ZZ_pX, vec_vec_ZZ_pX, mat_ZZ_pX)

static int failures = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
                   #cond);                                                \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static std::string str(const ZZ& a) {
  std::ostringstream os;
  os << a;
  return os.str();
}

static ZZ zz(const char* s) {
  ZZ r;
  std::istringstream is(s);
  is >> r;
  return r;
}

int main() {
  CHECK(std::string(NTL_VERSION) == "5.5.2");
  CHECK(NTL_BITS_PER_LONG == (int)(sizeof(long) * CHAR_BIT));
  CHECK(NTL_MAX_LONG == LONG_MAX);

  ZZ m127 = power(to_ZZ(2), 127) - 1;
  CHECK(str(m127) == "170141183460469231731687303715884105727");
  CHECK(ProbPrime(m127));
  CHECK(!ProbPrime(power(to_ZZ(2), 128) - 1));
  CHECK(zz("170141183460469231731687303715884105727") == m127);
  CHECK(NumBits(m127) == 127);

  // Fermat: 3^(p-1) = 1 mod p.
  CHECK(IsOne(PowerMod(to_ZZ(3), m127 - 1, m127)));
  CHECK(PowerMod(to_ZZ(4), to_ZZ(13), to_ZZ(497)) == 445);

  ZZ a = zz("123456789012345678901234567890");
  ZZ b = zz("987654321098765432109876543210");
  CHECK(str(GCD(a, b)) == "9000000000900000000090");
  CHECK(str(a * b) == "121932631137021795226185032733622923332237463801111263526900");
  ZZ q, r;
  DivRem(q, r, b, a);
  CHECK(q == 8 && str(r) == "9000000000900000000090");

  ZZ inv = InvMod(to_ZZ(17), to_ZZ(3120));
  CHECK(inv == 2753);

  // RSA-like round trip.
  ZZ p = zz("61"), qq = zz("53"), n = p * qq;
  ZZ c = PowerMod(to_ZZ(65), to_ZZ(17), n);
  CHECK(c == 2790);
  CHECK(PowerMod(c, inv, n) == 65);

  ZZ sq = SqrRoot(m127 * m127);
  CHECK(sq == m127);

  // RR
  RR::SetPrecision(200);
  RR x = to_RR(2);
  RR s = sqrt(x);
  CHECK(abs(s * s - x) < to_RR(1e-50));

  // ZZX factorization: (x - 1)(x + 2)(x^2 + 1) = x^4 + x^3 - x^2 + x - 2
  ZZX f;
  SetCoeff(f, 4, 1);
  SetCoeff(f, 3, 1);
  SetCoeff(f, 2, -1);
  SetCoeff(f, 1, 1);
  SetCoeff(f, 0, -2);
  ZZ content;
  vec_pair_ZZX_long factors;
  factor(content, factors, f);
  CHECK(content == 1);
  CHECK(factors.length() == 3);
  ZZX prod;
  set(prod);
  for (long i = 0; i < factors.length(); i++)
    for (long e = 0; e < factors[i].b; e++) prod *= factors[i].a;
  CHECK(prod == f);

  // LLL on a small lattice.
  mat_ZZ B;
  B.SetDims(3, 3);
  B[0][0] = 1; B[0][1] = 1; B[0][2] = 1;
  B[1][0] = -1; B[1][1] = 0; B[1][2] = 2;
  B[2][0] = 3; B[2][1] = 5; B[2][2] = 6;
  ZZ det2;
  long rank = LLL(det2, B);
  CHECK(rank == 3);
  CHECK(det2 == 9);
  ZZ n0 = B[0][0] * B[0][0] + B[0][1] * B[0][1] + B[0][2] * B[0][2];
  CHECK(n0 <= 2);

  mat_ZZ B2;
  B2.SetDims(2, 2);
  B2[0][0] = 201; B2[0][1] = 37;
  B2[1][0] = 1648; B2[1][1] = 297;
  LLL_FP(B2);
  ZZ d = abs(B2[0][0] * B2[1][1] - B2[0][1] * B2[1][0]);
  CHECK(d == 1279);
  mat_ZZ B3;
  B3.SetDims(2, 2);
  B3[0][0] = 201; B3[0][1] = 37;
  B3[1][0] = 1648; B3[1][1] = 297;
  LLL_QP(B3);
  CHECK(B3 == B2);

  // CrypTool-style mat_ZZX / vec_vec_ZZ_pX usage.
  mat_ZZX MX;
  MX.SetDims(2, 2);
  SetCoeff(MX[0][0], 1, 3);
  MX[1][1] = f;
  CHECK(deg(MX[1][1]) == 4 && coeff(MX[0][0], 1) == 3);
  vec_ZZX row = MX[1];
  CHECK(row.length() == 2);

  ZZ_p::init(to_ZZ(101));
  mat_ZZ_pX MP;
  MP.SetDims(1, 1);
  SetCoeff(MP[0][0], 2, 5);
  ZZ_pX g = MP[0][0] * MP[0][0];
  CHECK(deg(g) == 4 && rep(coeff(g, 4)) == 25);

  if (failures) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("NTL %s smoke test passed (NTL_BITS_PER_LONG=%d)\n", NTL_VERSION, NTL_BITS_PER_LONG);
  return EXIT_SUCCESS;
}

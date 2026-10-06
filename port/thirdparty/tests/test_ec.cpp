#include "ECsecude.h"
#include "s_ecFp.h"
#include "s_ecconv.h"
#include "ecssa.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef CT_HAVE_SECUDE_COMPAT
#define CT_HAVE_SECUDE_COMPAT 0
#endif

#if CT_HAVE_SECUDE_COMPAT

static int failures = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
                   #cond);                                                   \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

static std::string hex_of(char *s) {
  std::string out;
  if (!s)
    return "<null>";
  const char *p = s;
  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
    p += 2;
  for (; *p; ++p)
    if (std::isxdigit((unsigned char)*p))
      out += (char)std::toupper((unsigned char)*p);
  std::size_t nz = out.find_first_not_of('0');
  out = nz == std::string::npos ? "0" : out.substr(nz);
  std::free(s);
  return out;
}

int main() {
  // Same wiring CrypToolApp::InitInstance performs for libec.
#define DoOneFn(a, b, c, d) ECSecudeLib.c = c;
#define DoOneData(a, b) ECSecudeLib.b = &b;
  DoECAll
#undef DoOneFn
#undef DoOneData

  __CurveFp_struct curve;
  __PointAc_struct G, R, S, T;
  L_NUMBER cofactor[MAXLGTH], order[MAXLGTH];
  char id[] = "prime192v1";
  CHECK(X9_62_init_curve_Fp(&curve, &G, cofactor, order, id) == 0);
  CHECK(hex_of(ecFp_point_getstr_xcoord_ac(&G, 16)) == "188DA80EB03090F67CBF20EB43A18800F4FF0AFD82FF1012");
  CHECK(hex_of(ecFp_point_getstr_ycoord_ac(&G, 16)) == "7192B95FFC8DA78631011ED6B24CDD573F977A11E794811");

  CHECK(ecFp_mult_ac_str(&R, &curve, "2", &G) == 0);
  CHECK(hex_of(ecFp_point_getstr_xcoord_ac(&R, 16)) == "DAFEBF5828783F2AD35534631588A3F629A70FB16982A888");
  CHECK(hex_of(ecFp_point_getstr_ycoord_ac(&R, 16)) == "DD6BDA0D993DA0FA46B27BBC141B868F59331AFA5C7E93AB");

  CHECK(ecFp_mult_ac_str(&S, &curve, "0x1234567890abcdef1234567890abcdef", &G) == 0);
  CHECK(hex_of(ecFp_point_getstr_xcoord_ac(&S, 16)) == "BEDAC67FB3E84EB498A1703B7F3FB0A0AAAB39CA554C2CE9");
  CHECK(hex_of(ecFp_point_getstr_ycoord_ac(&S, 16)) == "C19EC031FB537035B750105FC0BBE37E839D65DED6595639");

  CHECK(ecFp_add_ac(&T, &curve, &G, &G) == 0);
  CHECK(hex_of(ecFp_point_getstr_xcoord_ac(&T, 16)) == "DAFEBF5828783F2AD35534631588A3F629A70FB16982A888");

  CHECK(ecFp_mult_ac(&T, &curve, order, &G) == 0);
  CHECK(ecFp_point_check_infinity_ac(&T));

  if (failures == 0)
    std::printf("libec: all checks passed\n");
  return failures == 0 ? 0 : 1;
}

#else

int main() {
  std::printf("libec: secude_compat target not available, headers compiled only\n");
  return 77;
}

#endif

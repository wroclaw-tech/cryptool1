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
  // prime192v1..v3 cannot be used: their literals in s_ecpcur.c contain TABs,
  // which libec's string_to_ln() rejects (dead code for CrypTool, also on Windows);
  // string_to_ln() also only accepts upper-case hex digits.
  char id[] = "prime256v1";
  CHECK(X9_62_init_curve_Fp(&curve, &G, cofactor, order, id) == 0);
  CHECK(hex_of(ecFp_point_getstr_xcoord_ac(&G, 16)) == "6B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296");
  CHECK(hex_of(ecFp_point_getstr_ycoord_ac(&G, 16)) == "4FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5");

  CHECK(ecFp_mult_ac_str(&R, &curve, "2", &G) == 0);
  CHECK(hex_of(ecFp_point_getstr_xcoord_ac(&R, 16)) == "7CF27B188D034F7E8A52380304B51AC3C08969E277F21B35A60B48FC47669978");
  CHECK(hex_of(ecFp_point_getstr_ycoord_ac(&R, 16)) == "7775510DB8ED040293D9AC69F7430DBBA7DADE63CE982299E04B79D227873D1");

  CHECK(ecFp_mult_ac_str(&S, &curve, "0X1234567890ABCDEF1234567890ABCDEF", &G) == 0);
  CHECK(hex_of(ecFp_point_getstr_xcoord_ac(&S, 16)) == "F9EBE464147CC102D92324D099D927C1A50E42D57A08A116FFEC0A29819D8C65");
  CHECK(hex_of(ecFp_point_getstr_ycoord_ac(&S, 16)) == "70BAF7D12888902A6A0C1D67ADD103123C859AB69BF8E9431C9384930D4B7EEE");

  CHECK(ecFp_add_ac(&T, &curve, &G, &G) == 0);
  CHECK(hex_of(ecFp_point_getstr_xcoord_ac(&T, 16)) == "7CF27B188D034F7E8A52380304B51AC3C08969E277F21B35A60B48FC47669978");

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

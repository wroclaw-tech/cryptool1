// Smoke test for apfloat_compat, following CrypTool's DlgComputeMersenneNumbers usage.
#include "ap.h"
#include "apint.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>

static int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
                   #cond);                                                 \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

struct MersenneNumberComputationParameters {
  apint base;
  unsigned long exponent;
  std::string result;
} params;

static unsigned computeMersenneNumber(void *_parameters) {
  MersenneNumberComputationParameters *parameters = (MersenneNumberComputationParameters *)(_parameters);
  apbase(10);
  apint base = parameters->base;
  unsigned long exp = parameters->exponent;
  apint r = base;
  int b2pow = 0;
  if (!exp) return 1;
  while (!(exp & 1)) {
    b2pow++;
    exp >>= 1;
  }
  while (exp >>= 1) {
    base *= base;
    if (exp & 1) r *= base;
  }
  while (b2pow--) {
    r *= r;
  }
  apint result = r - 1;
  std::ostringstream buffer;
  buffer << result;
  parameters->result = buffer.str();
  return 0;
}

static std::string mersenne(const char *base, unsigned long exponent) {
  params.base = (char *)base;
  params.exponent = strtoul(std::to_string(exponent).c_str(), NULL, 10);
  params.result = "";
  std::thread worker(computeMersenneNumber, (void *)&params);
  worker.join();
  return params.result;
}

static std::string str(const apint &x) {
  std::ostringstream os;
  os << x;
  return os.str();
}

int main() {
  CHECK(apinit());

  CHECK(mersenne("2", 127) == "170141183460469231731687303715884105727");
  CHECK(mersenne("2", 10) == "1023");
  CHECK(mersenne("3", 4) == "80");
  CHECK(mersenne("10", 1) == "9");
  std::string m521 = mersenne("2", 521);
  CHECK(m521.size() == 157);
  CHECK(m521.substr(0, 10) == "6864797660" && m521.substr(147) == "1115057151");
  std::string m44497 = mersenne("2", 44497);
  CHECK(m44497.size() == 13395);
  CHECK(m44497.substr(0, 12) == "854509824303" && m44497.substr(13395 - 12) == "961011228671");

  apint a = 1;
  a <<= 127;
  a -= 1;
  apbase(16);
  CHECK(str(a) == "7fffffffffffffffffffffffffffffff");
  apint h = (char *)"ff";
  CHECK(h == 255);
  apbase(2);
  CHECK(str(apint(10)) == "1010");
  apbase(10);

  CHECK(str(apint()) == "0");
  CHECK(str(-apint(42)) == "-42");
  CHECK(str(apint("1.5e3")) == "1500");
  CHECK(str(apint("-1.5")) == "-2");
  CHECK(str(apint(-2.5)) == "-3");
  CHECK(str(apint("  +123456789012345678901234567890")) == "123456789012345678901234567890");
  CHECK(str(apint(-7) / 2) == "-3");
  CHECK(str(apint(-7) % 2) == "-1");
  CHECK(str(100 - apint(1)) == "99");
  CHECK(str(apint(1LL << 40) * 3u) == "3298534883328");
  CHECK(str(apint(-(1LL << 62))) == "-4611686018427387904");
  CHECK(str(apint(~0ULL)) == "18446744073709551615");
  CHECK(apint(5) > 4 && 4 < apint(5) && apint(5) >= 5.0 && apint(5) != 4.5);
  CHECK(pow(apint(3), 5) == 243);
  CHECK(powmod(apint(3), apint(200), apint(1000)) == 1);
  CHECK(gcd(apint(84), apint(36)) == 12 && lcm(apint(4), apint(6)) == 12);
  apdiv_t qr = div(apint(17), apint(5));
  CHECK(qr.quot == 3 && qr.rem == 2);
  CHECK(abs(apint(-9)) == 9);
  apint i = 5;
  CHECK((i++) == 5 && i == 6 && (--i) == 5);

  std::istringstream in("31415926535897932384626");
  apint parsed;
  in >> parsed;
  CHECK(str(parsed) == "31415926535897932384626");

  apdeinit();

  if (failures) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  std::printf("test_apfloat: OK (2^127-1 = %s)\n", mersenne("2", 127).c_str());
  return 0;
}

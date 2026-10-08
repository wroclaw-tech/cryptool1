#include <big.h>
#include <crt.h>

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

static int failures = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
                   #cond);                                                   \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

static std::string to_dec(const Big &b) {
  std::ostringstream os;
  os << b;
  return os.str();
}

static Big from_dec(const char *s) {
  char buf[512];
  std::strncpy(buf, s, sizeof buf - 1);
  buf[sizeof buf - 1] = 0;
  return Big(buf);
}

static Big pollard_rho(const Big &n) {
  Big x = 2, y = 2, d = 1;
  int c = 1;
  while (d == 1) {
    x = (x * x + c) % n;
    y = (y * y + c) % n;
    y = (y * y + c) % n;
    d = gcd(x > y ? x - y : y - x, n);
  }
  return d;
}

static void run(miracl *mip) {

  Big m127 = pow(Big(2), 127) - 1;
  CHECK(to_dec(m127) == "170141183460469231731687303715884105727");
  CHECK(prime(m127));
  CHECK(!prime(pow(Big(2), 128) + 1));
  CHECK(bits(m127) == 127);

  Big a = from_dec("123456789012345678901234567890");
  CHECK(pow(a, m127 - 1, m127) == 1);

  Big p = from_dec("1000000007");
  Big e = from_dec("65537");
  CHECK(to_dec(pow(Big(3), e, p)) == "754428556");
  Big inv = inverse(Big(65537), p);
  CHECK((inv * 65537) % p == 1);

  Big q1 = from_dec("1000003"), q2 = from_dec("998244353");
  Big n = q1 * q2;
  Big f = pollard_rho(n);
  CHECK(f == q1 || f == q2);
  CHECK(f * (n / f) == n);

  CHECK(to_dec(Big((long)-42)) == "-42");
  if (sizeof(long) == 8)
    CHECK(to_dec(Big((long)0x123456789ABL)) == "1250999896491");

  mip->IOBASE = 16;
  CHECK(to_dec(m127) == "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  mip->IOBASE = 10;

  big raw = mirvar(0);
  char in[] = "987654321987654321987654321";
  cinstr(raw, in);
  char out[128];
  cotstr(raw, out);
  CHECK(std::strcmp(out, in) == 0);
  mirkill(raw);

  mr_utype moduli[3] = {3, 5, 7};
  mr_utype residues[3] = {2, 3, 2};
  Crt chinese(3, moduli);
  CHECK(chinese.eval(residues) == 23);

  Big bm[2] = {from_dec("1000000007"), from_dec("998244353")};
  Big br[2] = {Big(5), Big(7)};
  Crt bigchinese(2, bm);
  Big x = bigchinese.eval(br);
  CHECK(x % bm[0] == 5 && x % bm[1] == 7);

  flash fx = mirvar(0), fy = mirvar(0), fz = mirvar(0);
  fconv(1, 4, fx);
  fadd(fx, fx, fy);
  fmul(fy, fy, fz);
  fconv(1, 4, fx);
  CHECK(fcomp(fz, fx) == 0);
  fdiv(fz, fy, fx);
  fsub(fy, fx, fz);
  CHECK(size(fz) == 0);
  mirkill(fz);
  mirkill(fy);
  mirkill(fx);
}

int main() {
  static_assert(sizeof(mr_small) == 4, "CrypTool expects 32-bit MIRACL digits");
  static_assert(sizeof(mr_large) == 8, "double-length type must be 64-bit");

  miracl *mip = mirsys(4096 / 32 + 128, 0);
  CHECK(mip != NULL);
  CHECK(mip == get_mip());
  mip->IOBASE = 10;
  run(mip);
  mirexit();

  if (failures == 0)
    std::printf("miracl: all checks passed\n");
  return failures == 0 ? 0 : 1;
}

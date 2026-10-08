// Known-answer tests for the AES candidate ciphers through the NIST API calls
// CrypTool's CoreCryptography.cpp makes. Build with CT_AES_ORACLE (and the
// original headers) to regenerate the reference table with --dump.
#include "mars.h"
#include "RC6.h"
#include "Rijndael-api-fst.h"
#include "Serpent.h"
#include "Twofish.h"

#ifdef long
#error "the AES wrapper headers leaked their long narrowing"
#endif

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef CT_AES_ORACLE
extern "C" const size_t ct_aes_mars_sizes[2];
extern "C" const size_t ct_aes_rc6_sizes[2];
extern "C" const size_t ct_aes_rijndael_sizes[2];
extern "C" const size_t ct_aes_serpent_sizes[2];
extern "C" const size_t ct_aes_twofish_sizes[2];
#endif

static int failures = 0;

static void fail(const std::string &what) {
  std::fprintf(stderr, "FAIL: %s\n", what.c_str());
  ++failures;
}

static std::string hex(const BYTE *p, int n) {
  static const char digits[] = "0123456789abcdef";
  std::string s;
  for (int i = 0; i < n; ++i) {
    s += digits[p[i] >> 4];
    s += digits[p[i] & 15];
  }
  return s;
}

static std::vector<BYTE> unhex(const char *s) {
  std::vector<BYTE> out;
  for (; s[0] && s[1]; s += 2) {
    unsigned v = 0;
    std::sscanf(s, "%2x", &v);
    out.push_back((BYTE)v);
  }
  return out;
}

template <class K, class C>
struct Api {
  const char *name;
  int (*makeKey)(K *, BYTE, int, char *);
  int (*cipherInit)(C *, BYTE, char *);
  int (*encrypt)(C *, K *, BYTE *, int, BYTE *);
  int (*decrypt)(C *, K *, BYTE *, int, BYTE *);
};

template <class K, class C>
static std::string run(const Api<K, C> &api, int keyBits, const char *keyHex, BYTE mode,
                       const char *ivHex, const std::vector<BYTE> &pt) {
  std::string label = std::string(api.name) + " key=" + keyHex + " mode=" +
                      std::to_string(mode) + " iv=" + (ivHex ? ivHex : "-");
  char keyBuf[80], ivBuf[40];
  std::snprintf(keyBuf, sizeof keyBuf, "%s", keyHex);
  std::snprintf(ivBuf, sizeof ivBuf, "%s", ivHex ? ivHex : "00000000000000000000000000000000");
  std::vector<BYTE> in(pt), ct(pt.size() + 16), back(pt.size() + 16);
  int bits = (int)pt.size() * 8;

  K key;
  C cipher;
  std::memset(&key, 0, sizeof key);
  std::memset(&cipher, 0, sizeof cipher);
  if (api.makeKey(&key, DIR_ENCRYPT, keyBits, keyBuf) != TRUE)
    fail(label + ": makeKey(encrypt)");
  if (api.cipherInit(&cipher, mode, ivBuf) != TRUE)
    fail(label + ": cipherInit(encrypt)");
  api.encrypt(&cipher, &key, in.data(), bits, ct.data());

  std::snprintf(keyBuf, sizeof keyBuf, "%s", keyHex);
  std::snprintf(ivBuf, sizeof ivBuf, "%s", ivHex ? ivHex : "00000000000000000000000000000000");
  std::memset(&key, 0, sizeof key);
  std::memset(&cipher, 0, sizeof cipher);
  if (api.makeKey(&key, DIR_DECRYPT, keyBits, keyBuf) != TRUE)
    fail(label + ": makeKey(decrypt)");
  if (api.cipherInit(&cipher, mode, ivBuf) != TRUE)
    fail(label + ": cipherInit(decrypt)");
  api.decrypt(&cipher, &key, ct.data(), bits, back.data());
  if (std::memcmp(back.data(), pt.data(), pt.size()) != 0)
    fail(label + ": decrypt(encrypt(x)) != x");
  return hex(ct.data(), (int)pt.size());
}

static const Api<keyInstanceMars, cipherInstanceMars> kMars = {
    "MARS", makeKeyMars, cipherInitMars, blockEncryptMars, blockDecryptMars};
static const Api<keyInstanceRC6, cipherInstanceRC6> kRC6 = {
    "RC6", makeKeyRC6, cipherInitRC6, blockEncryptRC6, blockDecryptRC6};
static const Api<keyInstanceRijndael, cipherInstanceRijndael> kRijndael = {
    "RIJNDAEL", makeKeyRijndael, cipherInitRijndael, blockEncryptRijndael, blockDecryptRijndael};
static const Api<keyInstanceSerpent, cipherInstanceSerpent> kSerpent = {
    "SERPENT", makeKeySerpent, cipherInitSerpent, blockEncryptSerpent, blockDecryptSerpent};
static const Api<keyInstanceTwofish, cipherInstanceTwofish> kTwofish = {
    "TWOFISH", makeKeyTwofish, cipherInitTwofish, blockEncryptTwofish, blockDecryptTwofish};

template <class K, class C>
static std::string runAny(int which, const Api<K, C> &api, int keyBits, const char *key,
                          BYTE mode, const char *iv, const std::vector<BYTE> &pt) {
  (void)which;
  return run(api, keyBits, key, mode, iv, pt);
}

static std::string dispatch(int alg, int keyBits, const char *key, BYTE mode, const char *iv,
                            const std::vector<BYTE> &pt) {
  switch (alg) {
  case 0: return runAny(alg, kMars, keyBits, key, mode, iv, pt);
  case 1: return runAny(alg, kRC6, keyBits, key, mode, iv, pt);
  case 2: return runAny(alg, kRijndael, keyBits, key, mode, iv, pt);
  case 3: return runAny(alg, kSerpent, keyBits, key, mode, iv, pt);
  default: return runAny(alg, kTwofish, keyBits, key, mode, iv, pt);
  }
}

static const char *const kAlgNames[] = {"MARS", "RC6", "RIJNDAEL", "SERPENT", "TWOFISH"};

// Published vectors (independent of the reference implementations above).
struct Kat {
  int alg, keyBits;
  const char *key, *pt, *ct;
};
static const Kat kPublished[] = {
    // FIPS-197 appendix C
    {2, 128, "000102030405060708090a0b0c0d0e0f", "00112233445566778899aabbccddeeff", "69c4e0d86a7b0430d8cdb78070b4c55a"},
    {2, 192, "000102030405060708090a0b0c0d0e0f1011121314151617", "00112233445566778899aabbccddeeff", "dda97ca4864cdfe06eaf70a0ec0d7191"},
    {2, 256, "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", "00112233445566778899aabbccddeeff", "8ea2b7ca516745bfeafc49904b496089"},
    // Twofish paper, all-zero key and plaintext
    {4, 128, "00000000000000000000000000000000", "00000000000000000000000000000000", "9f589f5cf6122c32b6bfec2f2ae8c35a"},
    {4, 192, "000000000000000000000000000000000000000000000000", "00000000000000000000000000000000", "efa71f788965bd4453f860178fc19101"},
    {4, 256, "0000000000000000000000000000000000000000000000000000000000000000", "00000000000000000000000000000000", "57ff739d4dc92c1bd7fc01700cc8216f"},
    // RC6 paper
    {1, 128, "00000000000000000000000000000000", "00000000000000000000000000000000", "8fc3a53656b1f778c129df4e9848a41e"},
    {1, 128, "0123456789abcdef0112233445566778", "02132435465768798a9bacbdcedfe0f1", "524e192f4715c6231f51f6367ea43f18"},
    {1, 192, "000000000000000000000000000000000000000000000000", "00000000000000000000000000000000", "6cd61bcb190b30384e8a3f168690ae82"},
    {1, 256, "0123456789abcdef0112233445566778899aabbccddeeff01032547698badcfe", "02132435465768798a9bacbdcedfe0f1", "c8241816f0d7e48920ad16a1674e5d48"},
};

// Generated with --dump from the unmodified sources built for i386 (32-bit
// long, i.e. the semantics of the original Win32 build).
struct Ref {
  int alg, keyBits, keyIdx, mode, ivIdx;
  const char *ct;
};
static const Ref kReference[] = {
#include "test_aes_candidates_ref.inc"
};

static const char *keyFor(int idx, int bits, std::string &buf) {
  static const char pattern[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
  static const char other[] = "2b7e151628aed2a6abf7158809cf4f3c762e7160f38b4da56a784d9045190cfe";
  if (idx == 0)
    buf.assign(bits / 4, '0');
  else
    buf.assign(idx == 1 ? pattern : other, bits / 4);
  return buf.c_str();
}

static const char *ivFor(int idx) {
  switch (idx) {
  case 0: return "00000000000000000000000000000000";
  default: return "0f0e0d0c0b0a09080706050403020100";
  }
}

static std::vector<BYTE> plaintext() {
  std::vector<BYTE> pt(48);
  for (int i = 0; i < 48; ++i)
    pt[i] = (BYTE)(((i % 16) * 0x11 + i / 16) & 0xff);
  return pt;
}

int main(int argc, char **argv) {
  bool dump = argc > 1 && std::strcmp(argv[1], "--dump") == 0;

#ifndef CT_AES_ORACLE
  if (ct_aes_mars_sizes[0] != sizeof(keyInstanceMars) || ct_aes_mars_sizes[1] != sizeof(cipherInstanceMars))
    fail("MARS struct layout differs between library and public header");
  if (ct_aes_rc6_sizes[0] != sizeof(keyInstanceRC6) || ct_aes_rc6_sizes[1] != sizeof(cipherInstanceRC6))
    fail("RC6 struct layout differs between library and public header");
  if (ct_aes_rijndael_sizes[0] != sizeof(keyInstanceRijndael) || ct_aes_rijndael_sizes[1] != sizeof(cipherInstanceRijndael))
    fail("Rijndael struct layout differs between library and public header");
  if (ct_aes_serpent_sizes[0] != sizeof(keyInstanceSerpent) || ct_aes_serpent_sizes[1] != sizeof(cipherInstanceSerpent))
    fail("Serpent struct layout differs between library and public header");
  if (ct_aes_twofish_sizes[0] != sizeof(keyInstanceTwofish) || ct_aes_twofish_sizes[1] != sizeof(cipherInstanceTwofish))
    fail("Twofish struct layout differs between library and public header");
#endif

  for (const Kat &k : kPublished) {
    std::vector<BYTE> pt = unhex(k.pt);
    std::string ct = dispatch(k.alg, k.keyBits, k.key, MODE_ECB, nullptr, pt);
    if (ct != k.ct)
      fail(std::string(kAlgNames[k.alg]) + " published KAT key=" + k.key + ": got " + ct + " want " + k.ct);
  }

  const std::vector<BYTE> pt = plaintext();
  const int modes[] = {MODE_ECB, MODE_CBC};
  size_t checked = 0;
  for (int alg = 0; alg < 5; ++alg)
    for (int keyBits : {128, 192, 256})
      for (int keyIdx = 0; keyIdx < 3; ++keyIdx)
        for (int mode : modes)
          for (int ivIdx = 0; ivIdx < (mode == MODE_CBC ? 2 : 1); ++ivIdx) {
            std::string keyBuf;
            const char *key = keyFor(keyIdx, keyBits, keyBuf);
            std::string ct = dispatch(alg, keyBits, key, (BYTE)mode, ivFor(ivIdx), pt);
            if (dump) {
              std::printf("{%d, %d, %d, %d, %d, \"%s\"},\n", alg, keyBits, keyIdx, mode, ivIdx, ct.c_str());
              continue;
            }
            bool found = false;
            for (const Ref &r : kReference)
              if (r.alg == alg && r.keyBits == keyBits && r.keyIdx == keyIdx && r.mode == mode && r.ivIdx == ivIdx) {
                found = true;
                ++checked;
                if (ct != r.ct)
                  fail(std::string(kAlgNames[alg]) + " reference key=" + key + " mode=" +
                       std::to_string(mode) + " iv=" + std::to_string(ivIdx) + ": got " + ct + " want " + r.ct);
              }
            if (!found)
              fail(std::string(kAlgNames[alg]) + ": no reference entry");
          }

  if (dump)
    return failures == 0 ? 0 : 1;
  if (failures == 0)
    std::printf("aes candidates: %zu reference vectors and %zu published KATs passed\n", checked,
                sizeof kPublished / sizeof kPublished[0]);
  return failures == 0 ? 0 : 1;
}

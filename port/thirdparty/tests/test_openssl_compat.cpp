#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <cstddef>
#include <string>

// Exactly how CrypTool/HashingOperations.cpp and DlgAbout.cpp include them.
namespace __SSL
{
#include "OpenSSL//md2.h"
#include "OpenSSL//md4.h"
#include "OpenSSL//md5.h"
#include "OpenSSL//ripemd.h"
#include "OpenSSL//sha.h"
}

namespace OPENSSL {
#include "crypto.h"
}

typedef void (*fp_Init_t)(void *context);
typedef void (*fp_Update_t)(void *context, void *data, unsigned long len);
typedef void (*fp_Final_t)(void *hash, void *context);

struct HashAlgorithmsFP {
  fp_Init_t fp_Init;
  fp_Update_t fp_Update;
  fp_Final_t fp_Final;
  void *Context;
  int ContextSize;
  int BitLength;
  const char *Name;
};

static HashAlgorithmsFP HAFP[] = {
  {(fp_Init_t)__SSL::MD2_Init, (fp_Update_t)__SSL::MD2_Update, (fp_Final_t)__SSL::MD2_Final, NULL, sizeof(__SSL::MD2_CTX), 128, "MD2"},
  {(fp_Init_t)__SSL::MD4_Init, (fp_Update_t)__SSL::MD4_Update, (fp_Final_t)__SSL::MD4_Final, NULL, sizeof(__SSL::MD4_CTX), 128, "MD4"},
  {(fp_Init_t)__SSL::MD5_Init, (fp_Update_t)__SSL::MD5_Update, (fp_Final_t)__SSL::MD5_Final, NULL, sizeof(__SSL::MD5_CTX), 128, "MD5"},
  {(fp_Init_t)__SSL::SHA_Init, (fp_Update_t)__SSL::SHA_Update, (fp_Final_t)__SSL::SHA_Final, NULL, sizeof(__SSL::SHA_CTX), 160, "SHA"},
  {(fp_Init_t)__SSL::SHA1_Init, (fp_Update_t)__SSL::SHA1_Update, (fp_Final_t)__SSL::SHA1_Final, NULL, sizeof(__SSL::SHA_CTX), 160, "SHA-1"},
  {(fp_Init_t)__SSL::RIPEMD160_Init, (fp_Update_t)__SSL::RIPEMD160_Update, (fp_Final_t)__SSL::RIPEMD160_Final, NULL, sizeof(__SSL::RIPEMD160_CTX), 160, "RIPEMD-160"},
  {(fp_Init_t)__SSL::SHA256_Init, (fp_Update_t)__SSL::SHA256_Update, (fp_Final_t)__SSL::SHA256_Final, NULL, sizeof(__SSL::SHA256_CTX), 256, "SHA-256"},
  {(fp_Init_t)__SSL::SHA512_Init, (fp_Update_t)__SSL::SHA512_Update, (fp_Final_t)__SSL::SHA512_Final, NULL, sizeof(__SSL::SHA512_CTX), 512, "SHA-512"},
};

extern "C" {
#define CT_LAYOUT_DECL(T) extern const size_t ct_real_sizeof_##T; extern const size_t ct_real_offsetof_##T;
CT_LAYOUT_DECL(MD4_CTX)
CT_LAYOUT_DECL(MD5_CTX)
CT_LAYOUT_DECL(RIPEMD160_CTX)
CT_LAYOUT_DECL(SHA_CTX)
CT_LAYOUT_DECL(SHA256_CTX)
CT_LAYOUT_DECL(SHA512_CTX)
const char *ct_real_openssl_version(void);
}

static int failures = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

static std::string hex(const unsigned char *p, size_t n) {
  static const char digits[] = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; ++i) {
    s += digits[p[i] >> 4];
    s += digits[p[i] & 15];
  }
  return s;
}

// HashingOperations::DoHash: fp_Init, one fp_Update, fp_Final into a malloc'ed context.
static std::string do_hash(int id, const std::string &msg) {
  HashAlgorithmsFP *h = &HAFP[id];
  h->Context = malloc(h->ContextSize);
  unsigned char out[64];
  h->fp_Init(h->Context);
  h->fp_Update(h->Context, (void *)msg.data(), (unsigned long)msg.size());
  h->fp_Final(out, h->Context);
  free(h->Context);
  h->Context = NULL;
  return hex(out, h->BitLength / 8);
}

// chunkHashInit/Update/Final with irregular chunk sizes.
static std::string chunk_hash(int id, const std::string &msg) {
  HashAlgorithmsFP *h = &HAFP[id];
  h->Context = malloc(h->ContextSize);
  unsigned char out[64];
  h->fp_Init(h->Context);
  size_t pos = 0, step = 1;
  while (pos < msg.size()) {
    size_t n = step < msg.size() - pos ? step : msg.size() - pos;
    h->fp_Update(h->Context, (void *)(msg.data() + pos), (unsigned long)n);
    pos += n;
    step = step * 3 % 97 + 1;
  }
  h->fp_Final(out, h->Context);
  free(h->Context);
  h->Context = NULL;
  return hex(out, h->BitLength / 8);
}

struct Kat {
  int id;
  const char *msg;
  const char *digest;
};

int main() {
  CHECK(sizeof(__SSL::MD4_CTX) == ct_real_sizeof_MD4_CTX && offsetof(__SSL::MD4_CTX, num) == ct_real_offsetof_MD4_CTX);
  CHECK(sizeof(__SSL::MD5_CTX) == ct_real_sizeof_MD5_CTX && offsetof(__SSL::MD5_CTX, num) == ct_real_offsetof_MD5_CTX);
  CHECK(sizeof(__SSL::RIPEMD160_CTX) == ct_real_sizeof_RIPEMD160_CTX && offsetof(__SSL::RIPEMD160_CTX, num) == ct_real_offsetof_RIPEMD160_CTX);
  CHECK(sizeof(__SSL::SHA_CTX) == ct_real_sizeof_SHA_CTX && offsetof(__SSL::SHA_CTX, num) == ct_real_offsetof_SHA_CTX);
  CHECK(sizeof(__SSL::SHA256_CTX) == ct_real_sizeof_SHA256_CTX && offsetof(__SSL::SHA256_CTX, md_len) == ct_real_offsetof_SHA256_CTX);
  CHECK(sizeof(__SSL::SHA512_CTX) == ct_real_sizeof_SHA512_CTX && offsetof(__SSL::SHA512_CTX, md_len) == ct_real_offsetof_SHA512_CTX);

  static const char digits80[] =
      "12345678901234567890123456789012345678901234567890123456789012345678901234567890";
  static const char abc448[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  static const Kat kats[] = {
    {0, "", "8350e5a3e24c153df2275c9f80692773"},
    {0, "a", "32ec01ec4a6dac72c0ab96fb34c0b5d1"},
    {0, "abc", "da853b0d3f88d99b30283a69e6ded6bb"},
    {0, "message digest", "ab4f496bfb2a530b219ff33031fe06b0"},
    {0, "abcdefghijklmnopqrstuvwxyz", "4e8ddff3650292ab5a4108c3aa47940b"},
    {0, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "da33def2a42df13975352846c30338cd"},
    {0, digits80, "d5976f79d83d3a0dc9806c3c66f3efd8"},
    {1, "", "31d6cfe0d16ae931b73c59d7e0c089c0"},
    {1, "abc", "a448017aaf21d8525fc10ae87aa6729d"},
    {1, digits80, "e33b4ddc9c38f2199c3e7b164fcc0536"},
    {2, "", "d41d8cd98f00b204e9800998ecf8427e"},
    {2, "abc", "900150983cd24fb0d6963f7d28e17f72"},
    {2, digits80, "57edf4a22be3c955ac49da2e2107b67a"},
    {3, "abc", "0164b8a914cd2a5e74c4f7ff082c4d97f1edf880"},
    {3, abc448, "d2516ee1acfa5baf33dfc1c471e438449ef134c8"},
    {4, "abc", "a9993e364706816aba3e25717850c26c9cd0d89d"},
    {4, abc448, "84983e441c3bd26ebaae4aa1f95129e5e54670f1"},
    {5, "", "9c1185a5c5e9fc54612808977ee8f548b2258d31"},
    {5, "abc", "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc"},
    {6, "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
    {6, abc448, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
    {7, "abc", "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"},
  };

  for (const Kat &k : kats) {
    std::string got = do_hash(k.id, k.msg);
    if (got != k.digest) {
      fprintf(stderr, "%s(\"%.20s\") = %s, expected %s\n", HAFP[k.id].Name, k.msg, got.c_str(), k.digest);
      ++failures;
    }
  }

  std::string million(1000000, 'a');
  CHECK(do_hash(3, million) == "3232affa48628a26653b5aaa44541fd90d690603");
  CHECK(do_hash(4, million) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");

  std::string sample;
  for (int i = 0; i < 5000; ++i)
    sample += (char)(i * 131 + (i >> 3));
  for (int id = 0; id < 8; ++id)
    CHECK(do_hash(id, sample) == chunk_hash(id, sample));

  const char *version = OPENSSL::SSLeay_version(SSLEAY_VERSION);
  CHECK(version != NULL && strncmp(version, "OpenSSL 3", 9) == 0);
  CHECK(strcmp(version, ct_real_openssl_version()) == 0);
  CHECK(strcmp(__SSL::MD2_options(), "md2(int)") == 0);

  if (failures == 0)
    printf("openssl_compat: all checks passed (%s)\n", version);
  return failures == 0 ? 0 : 1;
}

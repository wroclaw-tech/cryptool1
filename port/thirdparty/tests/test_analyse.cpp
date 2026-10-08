#include "libanalyse/la_string.h"
#include "libanalyse/analyse.h"
#include "libanalyse/Des.h"

#include <cmath>
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

static std::string written(const SymbolArray &a) {
  std::ostringstream os;
  a.Write(os);
  return os.str();
}

int main() {
  String s("Krypto");
  s += "Tool";
  CHECK(s.length() == 10);
  CHECK(std::strcmp(upcase(s).chars(), "KRYPTOTOOL") == 0);
  CHECK(s.index("Tool") == 6);
  CHECK(std::strcmp(common_prefix(s, String("Kryptologie")).chars(), "Krypto") == 0);

  AppConverter conv;
  char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  conv.SetAlphabet(alphabet, 1);
  CHECK(conv.IsInAlphabet('a') && conv.IsInAlphabet('Z') && !conv.IsInAlphabet('1'));

  SymbolArray text(conv);
  text.ReadString("Hello, World!");
  CHECK(text.GetSize() == 10);
  CHECK(written(text) == "HELLOWORLD");

  SymbolArray key(conv);
  key.ReadString("D");
  text += key;
  CHECK(written(text) == "KHOORZRUOG");
  text -= key;
  CHECK(written(text) == "HELLOWORLD");

  SymbolArray vkey(conv);
  vkey.ReadString("KEY");
  text += vkey;
  CHECK(written(text) == "RIJVSUYVJN");
  text -= vkey;
  CHECK(written(text) == "HELLOWORLD");

  SymbolArray every2 = text.Extract(0, 2);
  CHECK(written(every2) == "HLOOL");

  SymbolArray sample(conv);
  sample.ReadString("ABABABABABABABABABAB");
  NGram ng(sample);
  CHECK(std::fabs(ng.Entropie() - 1.0) < 1e-9);
  CHECK(std::fabs(ng.Koinzidenz() - 0.5) < 0.05);

  std::ostringstream shown;
  {
    OStream out(shown);
    ng.Show(out << OStream::Title(0) << OStream::Description(0) << OStream::Summary(0));
  }
  CHECK(!shown.str().empty());

  DES des(0x13345779UL, 0x9BBCDFF1UL);
  SimpleArray<Symbol, 2> block;
  block[0] = 0x01234567UL;
  block[1] = 0x89ABCDEFUL;
  des(block, Cipher::Encrypt);
  CHECK(block[0] == 0x85E81354UL && block[1] == 0x0F0AB405UL);
  des(block, Cipher::Decrypt);
  CHECK(block[0] == 0x01234567UL && block[1] == 0x89ABCDEFUL);

  if (failures == 0)
    std::printf("libanalyse: all checks passed\n");
  return failures == 0 ? 0 : 1;
}

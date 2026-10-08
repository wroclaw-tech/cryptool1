// Smoke test for cryptool_cracklib using CrypTool's own declarations (passwordchecker.h).
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

#define STRINGSIZE 1024
#define NUMWORDS 16
#define MAXWORDLEN 32

struct pi_header {
	unsigned long int pih_magic;
	unsigned long int pih_numwords;
	unsigned short int pih_blocklen;
	unsigned short int pih_pad;
};

struct PWDICT {
	FILE *ifp;
	FILE *dfp;
	FILE *wfp;
	unsigned long int flags;
	unsigned long int hwms[256];
	struct pi_header header;
	int count;
	char data[NUMWORDS][MAXWORDLEN];
};

#define PW_WORDS(x) ((x)->header.pih_numwords)

extern "C" {
	unsigned long int FindPW(PWDICT *pwp, char *password);
	int PMatch(char *pattern, char *password);
	char *Trim(char *password);
	int PWClose(PWDICT *pwp);
	PWDICT *PWOpen(char *prefix, char *mode);

	void ct_cracklib_layout(size_t *size, size_t *numwords_offset, size_t *count_offset);
	char *ct_cracklib_getpw(PWDICT *pwp, unsigned long number);
	char *ct_cracklib_fascistcheck(char *password, char *path);
}

static int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
                   #cond);                                                 \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

static bool inDictionary(PWDICT *pwp, const char *word) {
  char buf[STRINGSIZE];
  std::snprintf(buf, sizeof buf, "%s", word);
  return FindPW(pwp, buf) != PW_WORDS(pwp);
}

int main() {
  size_t libSize = 0, libNumwordsOffset = 0, libCountOffset = 0;
  ct_cracklib_layout(&libSize, &libNumwordsOffset, &libCountOffset);
  CHECK(libSize == sizeof(PWDICT));
  CHECK(libNumwordsOffset == offsetof(PWDICT, header.pih_numwords));
  CHECK(libCountOffset == offsetof(PWDICT, count));

  char path[STRINGSIZE];
  std::snprintf(path, sizeof path, "%s", CT_CRACKLIB_DICT);
  char mode[] = "r";
  PWDICT *pwp = PWOpen(path, mode);
  CHECK(pwp != nullptr);
  if (!pwp) return 1;

  CHECK(pwp->header.pih_magic == 0x70775631UL);
  CHECK(PW_WORDS(pwp) == 1648594UL);
  CHECK(pwp->header.pih_blocklen == NUMWORDS);
  CHECK(pwp->flags & 0x0004);

  CHECK(inDictionary(pwp, "password"));
  CHECK(inDictionary(pwp, "dragon"));
  CHECK(inDictionary(pwp, "letmein"));
  CHECK(!inDictionary(pwp, "xq7#Zr!pW2vK"));

  char word[] = "dragon";
  unsigned long idx = FindPW(pwp, word);
  CHECK(idx < PW_WORDS(pwp));
  const char *back = ct_cracklib_getpw(pwp, idx);
  CHECK(back != nullptr && std::strcmp(back, "dragon") == 0);
  const char *last = ct_cracklib_getpw(pwp, PW_WORDS(pwp) - 1);
  CHECK(last != nullptr && last[0] != '\0');

  char pattern[] = "aadddddda";
  char ni[] = "ab123456c";
  CHECK(PMatch(pattern, ni) == 1);
  char padded[] = "  word  ";
  Trim(padded);
  CHECK(std::strcmp(padded, "  word") == 0);

  CHECK(PWClose(pwp) == 0);

  char weak[] = "password";
  char strong[] = "xq7#Zr!pW2vK";
  const char *weakReason = ct_cracklib_fascistcheck(weak, path);
  const char *strongReason = ct_cracklib_fascistcheck(strong, path);
  CHECK(weakReason != nullptr);
  CHECK(strongReason == nullptr);

  if (failures) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  std::printf("test_cracklib: OK (%lu words, \"password\" -> %s)\n", 1648594UL,
              weakReason ? weakReason : "(null)");
  return 0;
}

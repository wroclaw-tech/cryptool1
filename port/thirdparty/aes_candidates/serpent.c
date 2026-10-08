#include "ct_aes_prelude.h"

/* SERPENT.C converts its (narrowed) words with "%08lx"; keep the conversions
   on a real unsigned long. */
static int ct_serpent_sscanf(const char *str, const char *fmt, unsigned int *out)
{
  unsigned long value;
  int n = sscanf(str, fmt, &value);
  if (n == 1)
    *out = (unsigned int)value;
  return n;
}

static int ct_serpent_sprintf(char *buf, const char *fmt, unsigned int value)
{
  return sprintf(buf, fmt, (unsigned long)value);
}

#define sscanf ct_serpent_sscanf
#define sprintf ct_serpent_sprintf

/* The Serpent submission code assumes a 32-bit long. */
#define long int
#include "Serpent/SERPENT.C"
#undef long

const size_t ct_aes_serpent_sizes[2] = {sizeof(keyInstanceSerpent), sizeof(cipherInstanceSerpent)};

#include "ct_aes_prelude.h"

/* The IBM MARS code assumes a 32-bit long (WORD32). */
#define long int
#include "Mars/mars-opt.c"
#undef long

const size_t ct_aes_mars_sizes[2] = {sizeof(keyInstanceMars), sizeof(cipherInstanceMars)};

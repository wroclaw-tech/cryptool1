#include "ct_aes_prelude.h"

/* The RC6 reference code assumes a 32-bit long. */
#define long int
#include "RC6/rc6.c"
#undef long

const size_t ct_aes_rc6_sizes[2] = {sizeof(keyInstanceRC6), sizeof(cipherInstanceRC6)};

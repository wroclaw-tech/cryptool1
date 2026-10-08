#include "ct_aes_prelude.h"

#include "Twofish/TWOFISH2.C"

const size_t ct_aes_twofish_sizes[2] = {sizeof(keyInstanceTwofish), sizeof(cipherInstanceTwofish)};

#include "ct_aes_prelude.h"

#include "Rijndael/rijndael-api-fst.h"

const size_t ct_aes_rijndael_sizes[2] = {sizeof(keyInstanceRijndael), sizeof(cipherInstanceRijndael)};

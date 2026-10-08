// Kept free of SECUDE headers: crypt_p.h declares print_keyinfo_flag as
// unsigned char, but CrypTool accesses it through a sec_uint4 pointer, so the
// object must provide four bytes.
#include <cstdint>

extern "C" {
uint32_t print_cert_flag = 2 | 4 | 8 | 16 | 32 | 64 | 256 | 512 | 1024;
alignas(4) uint32_t print_keyinfo_flag = 1 | 4 | 8;
}

namespace compat {

uint32_t print_cert_flags() { return print_cert_flag; }
uint32_t print_keyinfo_flags() { return print_keyinfo_flag; }
void *print_cert_flag_address() { return &print_cert_flag; }
void *print_keyinfo_flag_address() { return &print_keyinfo_flag; }

} // namespace compat

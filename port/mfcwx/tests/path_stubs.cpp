#include <cstdio>
#include <string>

#include "mfcwx/codepage.h"

extern "C" FILE* mfcwx_fopen(const char* name, const char* mode) { return std::fopen(name, mode); }

namespace mfcwx {

std::string NativePath(const char* path) { return path ? path : ""; }

} // namespace mfcwx

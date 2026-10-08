// Source-compatible subset of apfloat 2.41's ap.h, backed by GMP (see apint.h).
#include <cstddef>
#include <iostream>
#include <fstream>

#if !defined(__AP_H)
#define __AP_H

#include <cassert>

#define APFLOAT_COMPAT 1
#define APFLOAT_COMPAT_VERSION "2.41-compat"

bool apinit (void);
void apdeinit (void);
void apbase (int digit);

#endif  // __AP_H

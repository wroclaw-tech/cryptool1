#pragma once

// Force-included into third-party libraries that open files by paths coming from CrypTool,
// which are in Windows form ("\dir\file").

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
FILE* mfcwx_fopen(const char* name, const char* mode);
#ifdef __cplusplus
}
#include "mfcwx/fstreamcompat.h"
#endif

#define fopen mfcwx_fopen

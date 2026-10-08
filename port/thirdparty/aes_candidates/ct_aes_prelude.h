#ifndef CT_AES_PRELUDE_H
#define CT_AES_PRELUDE_H

/* Every system header the AES candidate sources use, included before
   "long" is narrowed so that system declarations keep their real types. */
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <memory.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(__linux__)
#include <endian.h>
#endif

#ifndef LittleEndian
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define LittleEndian 0
#else
#define LittleEndian 1
#endif
#endif
#ifndef ALIGN32
#define ALIGN32 0
#endif

#endif

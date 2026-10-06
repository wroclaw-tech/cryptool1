/*
 *   MIRACL compiler/hardware definitions - mirdef.h
 *   Portable C configuration for the CrypTool port (GCC/Clang, 32- and 64-bit).
 *
 *   Same word size as the original Win32 build (32-bit mr_small, 64-bit
 *   double-length type), but without any inline assembly.
 */

#ifndef CRYPTOOL_MIRDEF_H
#define CRYPTOOL_MIRDEF_H

#define MIRACL 32

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define MR_BIG_ENDIAN
#else
#define MR_LITTLE_ENDIAN
#endif

#define mr_utype int
#define MR_IBITS 32
#if defined(__LP64__) || defined(_LP64)
#define MR_LBITS 64
#else
#define MR_LBITS 32
#endif
#define mr_unsign32 unsigned int
#define mr_dltype long long
#define mr_unsign64 unsigned long long
#define MR_NOASM
#define MR_FLASH 52
#define MR_NO_STANDARD_IO

/* glibc's <math.h> (C23 / _GNU_SOURCE, always on in g++) declares fadd, fsub, fmul
   and fdiv; rename MIRACL's flash functions after <math.h> has been seen. */
#ifdef __cplusplus
extern "C++" {
#include <math.h>
}
#else
#include <math.h>
#endif
#define fadd mr_fadd
#define fsub mr_fsub
#define fmul mr_fmul
#define fdiv mr_fdiv

#endif
